#include "mandelbrot.hpp"
#include <iostream>
#include <cassert>
#include <GL/gl.h>
#include <GL/glut.h>


inline mb::Cell::Cell() : x(0), y(0), m_escaped(false), m_iterations(0)
{
}

inline void mb::Cell::iterate(double x0, double y0)
{
	if( !m_escaped )
	{
		double xx = x*x, yy=y*y;
		m_escaped = xx+yy>4.0;
		if( !m_escaped )
		{
			double x1 = xx - yy + x0;
			double y1 = 2*x*y + y0;
			x=x1;
			y=y1;
			++m_iterations;
		}
	}
}

inline int mb::Cell::iterations() const
{
	return m_escaped ? m_iterations : 0;
}

inline bool mb::Cell::escaped() const
{
	return m_escaped;
}

int mb::Region::framebuffer_offset(int x, int y, int bpp) const
{
	if( x<0 || x>= width || y<0 || y>=height ) return -1;
	return 3*(y*width + x);
}

int mb::Region::framebuffer_offset(double x, double y, int bpp) const
{
	return framebuffer_offset( int((x-x0)/dx), int((y-y0)/dy), bpp);
}

void mb::ComputeRegion::active_method( Region Region )
{
	std::vector<Cell> cells(Region.width*Region.height);
	m_cells = active::platform::move(cells);
	m_region = Region;
	m_totalIterations=0;
}

void mb::ComputeRegion::active_method( Iterate Iterate )
{
	Ready result;
	result.region = m_region;
	result.iterations.reserve(m_cells.size());
	result.min_iterations=1000000;
	result.max_iterations=0;
	result.completed=0;
	result.escapedCount=0;

	double x,y;
	int i,j;
	std::vector<Cell>::iterator cell = m_cells.begin();
	for(j=0,y=m_region.y0; j<m_region.height; ++j,y+=m_region.dy)
	{
		for(i=0,x=m_region.x0; i<m_region.width; ++i,x+=m_region.dx,++cell)
		{
			for(int i=0; i<Iterate.times && !cell->escaped(); ++i)
			{
				cell->iterate(x,y);
				if( cell->escaped() ) ++result.completed;
			}
			if( cell->escaped() )
			{
				++result.escapedCount;
				result.min_iterations = std::min( result.min_iterations, cell->iterations() );
				result.max_iterations = std::max( result.max_iterations, cell->iterations() );
			}
			result.iterations.push_back( cell->iterations() );
		}
	}

	m_totalIterations += Iterate.times;
	if( result.escapedCount==0 ) result.min_iterations = m_totalIterations;	// !!!!!
	result.totalIterations = m_totalIterations;

	Iterate.finished->send(active::platform::move(result));
}


void mb::RenderRegion::active_method( ComputeReady ComputeReady )
{
	active::sink<Ready>::sp notifier(m_notifier.lock());
	if( !notifier ) return;

	Ready ready = { ComputeReady.region };
	ready.buffer.resize(3*ComputeReady.iterations.size());
	ready.completed = ComputeReady.completed;
	ready.minIterations = ComputeReady.min_iterations;
	ready.escapedCount = ComputeReady.escapedCount;
	ready.totalIterations = ComputeReady.totalIterations;

	int i=0;
	std::vector<int>::const_iterator j = ComputeReady.iterations.begin();
	for(int y=0; y<ComputeReady.region.height; ++y)
	{
		for(int x=0; x<ComputeReady.region.width; ++x)
		{
			if( *j>0 )
			{
				int iterations = *j;
				while( iterations>1024 ) iterations>>=1;

				ready.buffer[i++] = (iterations) % 255;
				ready.buffer[i++] = (80+iterations) % 255;
				ready.buffer[i++] = (160+iterations) % 255;
			}
			else
			{
				ready.buffer[i++] = 0;
				ready.buffer[i++] = 0;
				ready.buffer[i++] = 0;
			}
			++j;
		}
	}

	notifier->send(active::platform::move(ready));
}


void mb::RenderRegion::active_method( SetNotifier SetNotifier )
{
	m_notifier = SetNotifier;
}

mb::View::View() : m_dismissed(false)
{
}

void mb::View::active_method( Start Start )
{
	if( m_dismissed ) return;

	m_region = Start.region;
	m_displayBuffer.resize( 3 * m_region.width * m_region.height );

	m_dismissed = false;
	m_update = Start.updater;
	m_minIterations = MAX_ITERATIONS;
	m_maxIterations = 0;
	m_tilesRemaining=TILES*TILES;
	m_maxUnescapedIterations = MAX_ITERATIONS;

	// Create compute and render tiles....

	int xstep = m_region.width / TILES;
	int ystep = m_region.height / TILES;
	for(int tx=0; tx<TILES; ++tx) for(int ty=0; ty<TILES; ++ty)
	{
		RenderRegion::ptr render(new RenderRegion);
		m_tiles[tx][ty].render = render;
		(*render)(shared_from_this());

		Region subregion = {
			m_region.x0 + tx*xstep*m_region.dx,
			m_region.y0 + ty*ystep*m_region.dy,
			m_region.dx,
			m_region.dy,
			xstep, ystep, tx, ty
			};

		ComputeRegion::ptr compute(new ComputeRegion);
		m_tiles[tx][ty].compute = compute;

		(*compute)(subregion);

		ComputeRegion::Iterate iterate={STEP,render};
		(*compute)(iterate);
	}
	std::cout << "[" << std::flush;
}


void mb::View::active_method( StartFromOtherView StartFromOtherView )
{
	if( m_dismissed ) return;
	m_region = StartFromOtherView.newSettings.region;
	m_update = StartFromOtherView.newSettings.updater;

	m_displayBuffer = active::platform::move(StartFromOtherView.newBuffer);
	m_dismissed = false;
	assert( !m_displayBuffer.empty() );
	m_minIterations = MAX_ITERATIONS;
	m_maxIterations = 0;
	m_tilesRemaining=64;
	m_maxUnescapedIterations = StartFromOtherView.maxIterations;

	int xstep = m_region.width / TILES;
	int ystep = m_region.height / TILES;
	for(int tx=0; tx<TILES; ++tx) for(int ty=0; ty<TILES; ++ty)
	{
		RenderRegion::ptr render(new RenderRegion);
		m_tiles[tx][ty].render = render;
		(*render)(shared_from_this());

		Region subregion = {
			m_region.x0 + tx*xstep*m_region.dx,
			m_region.y0 + ty*ystep*m_region.dy,
			m_region.dx,
			m_region.dy,
			xstep, ystep, tx, ty
			};

		ComputeRegion::ptr compute(new ComputeRegion);
		m_tiles[tx][ty].compute = compute;

		(*compute)(subregion);

		ComputeRegion::Iterate iterate = {std::min(STEP+StartFromOtherView.minIterations,1000),render};
		(*compute)(iterate);
	}
	std::cout << "[" << std::flush;
}

void mb::View::active_method( RenderReady RenderReady )
{
	if( m_dismissed ) return;
	active::sink<Update>::sp update(m_update.lock());
	if( !update ) return;

	assert( m_region.width>0 );
	assert( m_region.height>0 );
	assert( m_displayBuffer.size() == m_region.width * m_region.height * 3 );

	// Copy RenderReady.buffer into m_displayBuffer

	std::vector<unsigned char>::iterator dest = m_displayBuffer.begin() + 3* (
				RenderReady.reg.width * RenderReady.reg.offset_x +
				m_region.width * RenderReady.reg.height * RenderReady.reg.offset_y);
	std::vector<unsigned char>::iterator src = RenderReady.buffer.begin();

	for(int row=0; row<RenderReady.reg.height; ++row, src+=3*RenderReady.reg.width, dest+=3*m_region.width)
	{
		std::copy( src,  src + 3 * RenderReady.reg.width, dest);
	}


	Update up = { m_region.width, m_region.height, m_displayBuffer };

	update->send(up);

	int tx = RenderReady.reg.offset_x;
	int ty = RenderReady.reg.offset_y;
	if( RenderReady.escapedCount < 10000-THRESHOLD  && RenderReady.totalIterations<20*m_minIterations /*MAX_ITERATIONS */ )
	//if( RenderReady.completed>THRESHOLD || (RenderReady.escapedCount==0 && RenderReady.minIterations<MAX_ITERATIONS) )
	{
		ComputeRegion::Iterate iterate = {STEP,m_tiles[tx][ty].render};
		(*m_tiles[tx][ty].compute)( iterate );
	}
	else
	{
		--m_tilesRemaining;
		m_minIterations = std::min(m_minIterations, RenderReady.minIterations);
		m_maxIterations = std::max(m_maxIterations, RenderReady.minIterations);
		if( m_tilesRemaining==0 )
		{
			std::cout << "=]\nReady: min iterations=" << m_minIterations
					  << ", width=" << m_region.dx*m_region.width
					  << ", x=" << m_region.x0
					  << ", y=" << m_region.y0 << std::endl;
		}
		else std::cout << "=" << std::flush;
	}
}

void mb::View::active_method( ZoomTo ZoomTo )
{
	m_dismissed = true;

	if( m_displayBuffer.empty() )
	{
		// We got to this handler too quickly - we haven't even been initialized ourselves yet!
		(*ZoomTo.newView)(ZoomTo.newSettings);
	}
	else
	{
		StartFromOtherView start = { ZoomTo.newSettings };
		start.newBuffer.resize( 3 * m_region.width * m_region.height );

		double x,y;
		int i,j;

		std::vector<unsigned char>::iterator output=start.newBuffer.begin();
		for(j=0,y=ZoomTo.newSettings.region.y0; j<m_region.height; ++j,y+=ZoomTo.newSettings.region.dy)
		{
			for(i=0,x=ZoomTo.newSettings.region.x0; i<m_region.width; ++i,x+=ZoomTo.newSettings.region.dx)
			{
				int src = m_region.framebuffer_offset(x,y);
				if( src>=0 )
				{
					*output++ = m_displayBuffer[src++];
					*output++ = m_displayBuffer[src++];
					*output++ = m_displayBuffer[src++];
				}
				else
				{
					output+=3;
				}
			}
		}

		start.minIterations = m_minIterations;
		start.maxIterations = m_maxIterations;

		Update update = {m_region.width, m_region.height, start.newBuffer};
		ZoomTo.newSettings.updater->send( update );
		(*ZoomTo.newView)( start );
	}
}

void mb::View::active_method( Stop Stop )
{
	m_dismissed = true;
	for(int i=0; i<m_tilesRemaining; ++i) std::cout << "-";
	if( m_tilesRemaining ) std::cout << "]" << std::endl;
}

mb::GlutWindow::GlutWindow(int argc, char**argv)
{
	glutInit( &argc, argv );
	glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE );
	m_region.width=800;
	m_region.height=800;
	m_displayBuffer.resize( 3 * m_region.width * m_region.height );
	glutInitWindowSize(m_region.width, m_region.height);
	m_window = glutCreateWindow("Mandelbrot");

	glutDisplayFunc( Draw );
	glutKeyboardFunc( Keyboard );
	glutMouseFunc( Mouse );
	glutReshapeFunc(Reshape);
	glutIdleFunc(Idle);
	global = this;
}

void mb::GlutWindow::active_method( NavigateTo NavigateTo )
{
	m_region.x0 = NavigateTo.x0;
	m_region.y0 = NavigateTo.y0;
	m_region.dx = (NavigateTo.x1-NavigateTo.x0)/m_region.width;
	m_region.dy = (NavigateTo.y1-NavigateTo.y0)/m_region.height;
	m_region.offset_x=0;
	m_region.offset_y=0;

	m_displayBuffer.resize( 3 * m_region.width * m_region.height );
	std::fill( m_displayBuffer.begin(), m_displayBuffer.end(), 0 );

	if( view )
	{
		(*view)(View::Stop());
	}

	View::Start start = { m_region, shared_from_this() };
	view.reset( new View );
	(*view)(start);
}

void mb::GlutWindow::active_method(Navigate Navigate)
{
	View::ptr new_view(new View());
	if( view )
	{
		(*view)(View::Stop());
		View::ZoomTo zoom = {m_region,shared_from_this(),new_view};
		(*view)(zoom);
		// Warning: This message could reach the view before it's even been properly initialized with Start/StartFromOther
	}
	else
	{
		View::Start start = {m_region, shared_from_this()};
		(*new_view)(start);
	}
	view = new_view;
}

void mb::GlutWindow::active_method( View::Update ViewUpdate )
{
	{
		active::platform::lock_guard<active::platform::mutex> lock(global->m_mutex);
		m_displayBuffer = active::platform::move(ViewUpdate.buffer);
	}
	glutPostWindowRedisplay(m_window);
}

void mb::GlutWindow::runMainLoop()
{
	try
	{
		glutMainLoop();
	}
	catch( exit_main_loop )
	{
	}
}

void mb::GlutWindow::OnIdle()
{
	// Stop being a CPU hog:
#if ACTIVE_USE_BOOST
	active::platform::this_thread::sleep(boost::posix_time::milliseconds(10));
#else
	std::this_thread::sleep_for( std::chrono::milliseconds(10) );
#endif
}

void mb::GlutWindow::Idle()
{
	global->OnIdle();
}

void mb::GlutWindow::OnReshape(int width, int height)
{
	glutReshapeWindow(800, 800);
}

void mb::GlutWindow::Reshape(int width, int height)
{
	global->OnReshape(width,height);
}

void mb::GlutWindow::Keyboard( unsigned char c, int x, int y )
{
	global->OnKeyboard(c,x,y);
}

void mb::GlutWindow::OnKeyboard(unsigned char c, int x, int y )
{
	const int KEY_SPACE = ' ';
	const int KEY_ESCAPE = '\033';

	switch( c )
	{
	case KEY_SPACE:
		{
			GlutWindow::NavigateTo nav = { -2.5, -2, 1.5, 2 };
			(*this)(nav);
		}
		break;
	case 'q':
	case KEY_ESCAPE:
		throw exit_main_loop();	// Hack to get GLUT to shut down properly.
		break;
	}
}

void mb::GlutWindow::Draw( void )
{
	global->OnDraw();
}

void mb::GlutWindow::OnDraw()
{
	// glClear(GL_COLOR_BUFFER_BIT);
	{
		active::platform::lock_guard<active::platform::mutex> lock(m_mutex);
		glDrawPixels( m_region.width, m_region.height,
					 GL_RGB, GL_UNSIGNED_BYTE, &m_displayBuffer[0]);
	}

	glutSwapBuffers();
}

void mb::GlutWindow::Mouse( int button, int state, int x, int y )
{
	global->OnMouse( button, state, x, y );
}

void mb::GlutWindow::OnMouse(int button, int state, int x, int y)
{
	if( state !=0 ) return;
	switch( button )
	{
	case GLUT_LEFT_BUTTON:
		{
			Region & r = m_region;
			double cx = r.x0 + x * r.dx;
			double cy = r.y0 + (r.height-y) * r.dy;

			r.dx*=SCALE;
			r.dy*=SCALE;
			r.x0 = cx - r.dx*r.width*0.5;
			r.y0 = cy - r.dy*r.height*0.5;
			(*this)(Navigate());
		}
		break;

	case GLUT_RIGHT_BUTTON:
		{
			Region & r = m_region;
			double cx = r.x0 + x * r.dx;
			double cy = r.y0 + (r.height-y) * r.dy;

			r.dx/=SCALE;
			r.dy/=SCALE;
			r.x0 = cx - r.dx*r.width*0.5;
			r.y0 = cy - r.dy*r.height*0.5;
			(*this)(Navigate());
		}
		break;
	}
}

mb::GlutWindow * mb::GlutWindow::global;


int main(int argc, char**argv)
{
	mb::GlutWindow::ptr win( new mb::GlutWindow(argc, argv) );
	mb::GlutWindow::NavigateTo navigate = { -2.5, -2, 1.5, 2 };
	(*win)(	navigate );
	active::run run;
	win->runMainLoop();	// Must be called from main thread - limitation of GLUT
}
