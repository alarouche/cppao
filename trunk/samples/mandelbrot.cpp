#include <active_object.hpp>
#include <vector>
#include <GL/gl.h>
#include <GL/glut.h>

#include <iostream>
#include <cassert>

const double SCALE=0.25;
static const int STEP=200;
static const int THRESHOLD=1;
static const int MAX_ITERATIONS=20000;


// !! Move into mandelbrot.h

struct Cell
{
	Cell() : x(0), y(0), m_escaped(false), m_iterations(0)
	{
	}

	void iterate(double x0, double y0)
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

	int iterations() const { return m_escaped ? m_iterations : 0; }
	bool escaped() const { return m_escaped; }

private:
	double x,y;
	bool m_escaped;
	int m_iterations;
};

struct Region
{
	double x0, y0, dx, dy;
	int width, height;
	int offset_x, offset_y;

	int framebuffer_offset(int x, int y, int bpp=3) const
	{
		if( x<0 || x>= width || y<0 || y>=height ) return -1;
		return 3*(y*width + x);
	}

	int framebuffer_offset(double x, double y, int bpp=3) const
	{
		return framebuffer_offset( int((x-x0)/dx), int((y-y0)/dy), bpp);
	}
};

struct ComputeTile : public active::shared<ComputeTile>
{
	ACTIVE_METHOD( Region )
	{
		std::vector<Cell> cells(Region.width*Region.height);
		m_cells.swap(cells);
		m_region = Region;
		m_totalIterations=0;
	}

	struct Ready
	{
		Region region;
		std::vector<int> iterations;
		int completed;
		int min_iterations, max_iterations;
		int escapedCount;
	};

	// Iterate a number of times
	struct Iterate
	{
		int times;
		active::sink<Ready>::sp finished;
	};

	ACTIVE_METHOD( Iterate )
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
		auto cell = m_cells.begin();
		for(j=0,y=m_region.y0; j<m_region.height; ++j,y+=m_region.dy)
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
		
		m_totalIterations += Iterate.times;
		if( result.escapedCount==0 ) result.min_iterations = m_totalIterations;
		
		std::cout << ".";
		(*Iterate.finished)(std::move(result));
	}

private:
	std::vector<Cell> m_cells;
	Region m_region;
	int m_totalIterations;
};

struct RenderTile : public active::shared<RenderTile>, public active::sink<ComputeTile::Ready>
{
	struct Ready
	{
		Region reg;
		std::vector<GLbyte> buffer;
		int completed;
		int minIterations;
		int escapedCount;
	};

	typedef ComputeTile::Ready ComputeReady;

	ACTIVE_METHOD( ComputeReady )
	{
		auto notifier(m_notifier.lock());
		if( !notifier ) return;

		Ready ready = { ComputeReady.region };
		ready.buffer.resize(3*ComputeReady.iterations.size());
		ready.completed = ComputeReady.completed;
		ready.minIterations = ComputeReady.min_iterations;
		ready.escapedCount = ComputeReady.escapedCount;

		int i=0;
		auto j = ComputeReady.iterations.cbegin();
		for(int y=0; y<ComputeReady.region.height; ++y)
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

		(*notifier)(std::move(ready));
	}


	typedef active::sink<Ready>::sp SetNotifier;

	ACTIVE_METHOD( SetNotifier )
	{
		m_notifier = SetNotifier;
	}

private:
	active::sink<Ready>::wp m_notifier;
};

struct View : public active::shared<View>, public active::sink<RenderTile::Ready>
{
// Messages:

	struct Update
	{
		int width;
		int height;
		std::vector<GLbyte> buffer;
	};

	struct Start
	{
		Region region;
		active::sink<Update>::sp updater;	// Whom to notify when contents have changed
	};

	struct StartFromOtherView
	{
		Start newSettings;
		std::vector<GLbyte> newBuffer;
		int minIterations[8][8];
	};

	// Destroy this view and start another view
	struct ZoomTo
	{
		Start newSettings;
		ptr newView;
	};

	struct Stop { };

// Handlers:

	ACTIVE_METHOD( ZoomTo )
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

			auto output=start.newBuffer.begin();
			for(j=0,y=ZoomTo.newSettings.region.y0; j<m_region.height; ++j,y+=ZoomTo.newSettings.region.dy)
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

			for(int tx=0; tx<8; ++tx)
				for(int ty=0; ty<8; ++ty)
				{

				}

			(*ZoomTo.newSettings.updater)( Update{m_region.width, m_region.height, start.newBuffer} );
			(*ZoomTo.newView)( start );
		}
	}

	ACTIVE_METHOD( StartFromOtherView )
	{
		if( m_dismissed ) return;
		m_region = StartFromOtherView.newSettings.region;
		m_update = StartFromOtherView.newSettings.updater;

		m_displayBuffer = std::move(StartFromOtherView.newBuffer);
		m_dismissed = false;
		assert( !m_displayBuffer.empty() );

		int xstep = m_region.width / 8;
		int ystep = m_region.height / 8;
		for(int tx=0; tx<8; ++tx)
			for(int ty=0; ty<8; ++ty)
			{
				auto render = std::make_shared<RenderTile>();
				m_tiles[tx][ty].render = render;
				(*render)(shared_from_this());

				Region subregion{
					m_region.x0 + tx*xstep*m_region.dx,
					m_region.y0 + ty*ystep*m_region.dy,
					m_region.dx,
					m_region.dy,
					xstep, ystep, tx, ty
					};

				auto compute = std::make_shared<ComputeTile>();
				m_tiles[tx][ty].compute = compute;

				(*compute)(subregion);

				(*compute)(ComputeTile::Iterate{StartFromOtherView.minIterations[tx][ty] + STEP,render});
			}
	}

	ACTIVE_METHOD( Start )
	{
		if( m_dismissed ) return;

		m_region = Start.region;
		m_displayBuffer.resize( 3 * m_region.width * m_region.height );

		m_dismissed = false;
		m_update = Start.updater;

		// Create compute and render tiles....

		int xstep = m_region.width / 8;
		int ystep = m_region.height / 8;
		for(int tx=0; tx<8; ++tx)
			for(int ty=0; ty<8; ++ty)
			{
				auto render = std::make_shared<RenderTile>();
				m_tiles[tx][ty].render = render;
				(*render)(shared_from_this());

				Region subregion{
					m_region.x0 + tx*xstep*m_region.dx,
					m_region.y0 + ty*ystep*m_region.dy,
					m_region.dx,
					m_region.dy,
					xstep, ystep, tx, ty
					};

				auto compute = std::make_shared<ComputeTile>();
				m_tiles[tx][ty].compute = compute;

				(*compute)(subregion);

				(*compute)(ComputeTile::Iterate{STEP,render});
			}
	}

	ACTIVE_METHOD( Stop )
	{
		m_dismissed = true;
	}

	typedef RenderTile::Ready RenderReady;

	ACTIVE_METHOD( RenderReady )
	{
		if( m_dismissed ) return;
		auto update(m_update.lock());
		if( !update ) return;

		assert( m_region.width>0 );
		assert( m_region.height>0 );
		assert( m_displayBuffer.size() == m_region.width * m_region.height * 3 );

		// Copy RenderReady.buffer into m_displayBuffer
		{
			auto dest = m_displayBuffer.begin() + 3* (
						RenderReady.reg.width * RenderReady.reg.offset_x +
						m_region.width * RenderReady.reg.height * RenderReady.reg.offset_y);
			auto src = RenderReady.buffer.begin();

			for(int row=0; row<RenderReady.reg.height; ++row, src+=3*RenderReady.reg.width, dest+=3*m_region.width)
			{
				std::copy( src,  src + 3 * RenderReady.reg.width, dest);
			}
		}

		Update up = { m_region.width, m_region.height, m_displayBuffer };

		(*update)(up);
		
		std::cout << "Escaped count=" << RenderReady.escapedCount << " min iterations=" << RenderReady.minIterations << std::endl;

		if( RenderReady.completed>THRESHOLD || (RenderReady.escapedCount==0 && RenderReady.minIterations<MAX_ITERATIONS) )
		{
			int tx = RenderReady.reg.offset_x;
			int ty = RenderReady.reg.offset_y;

			m_tiles[tx][ty].minIterations = RenderReady.minIterations;
			(*m_tiles[tx][ty].compute)( ComputeTile::Iterate{STEP,m_tiles[tx][ty].render} );

			// std::cout << RenderReady.completed << " left" << std::endl;
			//ComputeTile::Iterate iterate = { STEP, m_render };
			//(*m_tile)(iterate);
		}
		else
		{
			std::cout << "Ready" << std::endl;
		}
	}

	View() : m_dismissed(false) { }

private:
	Region m_region;
	bool m_dismissed;
	std::vector<GLbyte> m_displayBuffer;
	active::sink<Update>::wp m_update;
	int m_min_iterations;

	struct Tile
	{
		ComputeTile::ptr compute;
		RenderTile::ptr render;
		int minIterations;
	} m_tiles[8][8];
};


static const int KEY_SPACE = ' ';
static const int KEY_ESCAPE = '\033';


class GlutWindow :
		public active::shared<GlutWindow>,
		public active::sink<View::Update>
{
private:
	std::shared_ptr<View> view;
	Region m_region;
public:

	struct zoom_to
	{
		double x0, y0, x1, y1;
	};


	ACTIVE_METHOD( zoom_to )
	{
		m_region.x0 = zoom_to.x0;
		m_region.y0 = zoom_to.y0;
		m_region.dx = (zoom_to.x1-zoom_to.x0)/m_region.width;
		m_region.dy = (zoom_to.y1-zoom_to.y0)/m_region.height;
		m_region.offset_x=0;
		m_region.offset_y=0;

		m_displayBuffer.resize( 3 * m_region.width * m_region.height );
		std::fill( m_displayBuffer.begin(), m_displayBuffer.end(), 0 );

		if( view )
		{
			(*view)(View::Stop());
		}

		View::Start start = { m_region, shared_from_this() };
		view = std::make_shared<View>();
		(*view)(start);
	}
	struct zoom{};

	typedef View::Update view_update;

	ACTIVE_METHOD( view_update )
	{
		{
			std::lock_guard<std::mutex> lock(global->m_mutex);
			m_displayBuffer = std::move(view_update.buffer);
		}
		glutPostWindowRedisplay(win);
	}

	ACTIVE_METHOD(zoom)
	{
		// !! Actually want minimum iterations *IN THE REGION WE HAVE ZOOMED TO*

		auto new_view = std::make_shared<View>();
		if( view )
		{
			(*view)(View::Stop());
			(*view)(View::ZoomTo{m_region,shared_from_this(),new_view});
			// Warning: This message could reach the view before it's even been properly initialized with Start/StartFromOther
		}
		else
		{
			(*new_view)(View::Start{m_region, shared_from_this()});
		}
		view = new_view;
	}

	struct resize
	{
		int width, height;
	};

	ACTIVE_METHOD( resize )
	{
		m_region.width = resize.width;
		m_region.height = resize.height;

		// !! Now see what do to
	}

	GlutWindow(int argc, char**argv)
	{
		glutInit( &argc, argv );
		// full color, double buffered
		glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE );
		m_region.width=800;
		m_region.height=800;
		m_displayBuffer.resize( 3 * m_region.width * m_region.height );
		glutInitWindowSize(m_region.width, m_region.height);
		win = glutCreateWindow("Mandelbrot");

		glutDisplayFunc( Draw );
		glutKeyboardFunc( Keyboard );
		glutMouseFunc( Mouse );
		glutReshapeFunc(Reshape);
		glutIdleFunc(Idle);
		global = this;
	}

	struct exit_main_loop { };

	void runMainLoop()
	{
		// Non-active method runs concurrently with
		// active methods
		try
		{
			glutMainLoop();
		}
		catch( exit_main_loop )
		{
		}
	}

private:
	int win;
	static GlutWindow * global;

	// Because GLUT itself is not threadsafe and insists on running
	// in the main thread. Sigh, defeats the elegance of active objects really.
	std::mutex m_mutex;

	std::vector<GLbyte> m_displayBuffer;

	void OnIdle()
	{
		// Stop being a CPU hog
		std::this_thread::sleep_for( std::chrono::milliseconds(10) );
	}

	static void Idle()
	{
		global->OnIdle();
	}

	void OnReshape(int width, int height)
	{
		//global->m_region.width = width;
		// global->m_region.height = height;

		// glViewport(0, 0, width, height);

		//glMatrixMode(GL_PROJECTION);
		//glLoadIdentity();
		//gluOrtho2D(-175, 175, -175, 175);
		//glMatrixMode(GL_MODELVIEW);
	}

	static void Reshape(int width, int height)
	{
		global->OnReshape(width,height);
	}

	static void Keyboard( unsigned char c, int x, int y )
	{
		global->OnKeyboard(c,x,y);
	}

	void OnKeyboard(unsigned char c, int x, int y )
	{
		switch( c )
		{
			case KEY_SPACE:
				{
					(*this)(GlutWindow::zoom_to { -2.5, -2, 1.5, 2 });
				}
				break;
			case 'q':
			case KEY_ESCAPE:
				// this is really bad -- the program cannot
				// do proper cleanup etc.
				// the exception mechanism provides a much better
				// solution, but it still cannot be relied upon in
				// som compilers, so we do not use it
				// exit( 0 );
				throw exit_main_loop();
				break;
			default:
				// ignore any character not (yet) recognized
				;
		}
	}

	static void Draw( void )
	{
		global->OnDraw();
	}

	void OnDraw()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			glDrawPixels( m_region.width, m_region.height,
						 GL_RGB, GL_UNSIGNED_BYTE, &m_displayBuffer[0]);
		}

		glutSwapBuffers();
	}

	static void Mouse( int button, int state, int x, int y )
	{
		global->OnMouse( button, state, x, y );
	}

	void OnMouse(int button, int state, int x, int y)
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
				std::cout << "Clicked on " << cx << "," << cy << "\n";
				(*this)(zoom());
				// glutPostRedisplay();
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
				std::cout << "Clicked on " << cx << "," << cy << "\n";
				(*this)(zoom());
			}
			break;
		}
	}
};

GlutWindow * GlutWindow::global;


int main(int argc, char**argv)
{
	std::shared_ptr<GlutWindow> win = std::make_shared<GlutWindow>(argc, argv);
	GlutWindow::zoom_to z = { -2.5, -2, 1.5, 2 };
	(*win)(z);
	active::run run;
	win->runMainLoop();	// Must be called from main thread - limitation of GLUT
}
