#include <active_object.hpp>
#include <vector>
#include <GL/gl.h>
#include <GL/glut.h>

#include <iostream>

const double SCALE=0.25;
static const int STEP=200;
static const int THRESHOLD=1;


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
};

struct ComputeTile : public active::shared<ComputeTile>
{
	ACTIVE_METHOD( Region )
	{
		std::vector<Cell> cells(Region.width*Region.height);
		m_cells.swap(cells);
		m_region = Region;
	}

	struct Ready
	{
		Region region;
		std::vector<int> iterations;
		int completed;
		int min_iterations, max_iterations;
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
					result.min_iterations = std::min( result.min_iterations, cell->iterations() );
					result.max_iterations = std::max( result.max_iterations, cell->iterations() );
				}
				result.iterations.push_back( cell->iterations() );
			}

		(*Iterate.finished)(std::move(result));
	}

private:
	std::vector<Cell> m_cells;
	Region m_region;
};

struct RenderTile : public active::shared<RenderTile>, public active::sink<ComputeTile::Ready>
{
	struct Ready
	{
		Region reg;
		std::vector<GLbyte> buffer;
		int completed;
	};

	typedef ComputeTile::Ready ComputeReady;

	ACTIVE_METHOD( ComputeReady )
	{
		auto notifier(m_notifier.lock());
		if( !notifier ) return;

		Ready ready = { ComputeReady.region };
		ready.buffer.resize(3*ComputeReady.iterations.size());
		ready.completed = ComputeReady.completed;

		std::cout << "Minimum iterations = " << ComputeReady.min_iterations << "\n";
		std::cout << "Maximum iterations = " << ComputeReady.max_iterations << "\n";
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

	struct Stop { };

// Handlers:

	ACTIVE_METHOD( Start )
	{
		m_region = Start.region;

		// !! Get the initial display buffer from the start message
		m_displayBuffer.resize( 3 * m_region.width * m_region.height );

		m_dismissed = false;
		m_update = Start.updater;

		m_render = std::make_shared<RenderTile>();
		(*m_render)(shared_from_this());

		m_tile = std::make_shared<ComputeTile>();
		(*m_tile)(m_region);

		// !! Adaptive step
		ComputeTile::Iterate iterate = { STEP, m_render };
		(*m_tile)(iterate);
	}

	ACTIVE_METHOD( Stop )
	{
		m_dismissed = true;
	}

	typedef RenderTile::Ready RenderReady;

	ACTIVE_METHOD( RenderReady )
	{
		if( m_dismissed ) return;
		active::sink<Update>::sp update(m_update.lock());
		if( !update ) return;

		m_displayBuffer = std::move(RenderReady.buffer);

		Update up = { m_region.width, m_region.height, m_displayBuffer };

		(*update)(up);

		if( RenderReady.completed>THRESHOLD )
		{
			std::cout << RenderReady.completed << " left" << std::endl;
			ComputeTile::Iterate iterate = { STEP, m_render };
			(*m_tile)(iterate);
		}
		else
		{
			std::cout << "Ready" << std::endl;
		}
	}

private:
	Region m_region;
	bool m_dismissed;
	std::vector<GLbyte> m_displayBuffer;
	ComputeTile::ptr m_tile;
	RenderTile::ptr m_render;
	active::sink<Update>::wp m_update;
	int m_min_iterations;
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

		if( view )
		{
			(*view)(View::Stop());
		}

		View::Start start = { m_region, shared_from_this() };
		view = std::make_shared<View>();
		(*view)(start);
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

	static void Idle()
	{
		// std::cout << "Idle function called\n";
	}

	static void Reshape(int width, int height)
	{
		//global->m_region.width = width;
		// global->m_region.height = height;

		// glViewport(0, 0, width, height);

		//glMatrixMode(GL_PROJECTION);
		//glLoadIdentity();
		//gluOrtho2D(-175, 175, -175, 175);
		//glMatrixMode(GL_MODELVIEW);
	}

	static void
	Keyboard( unsigned char c, int x, int y )
	{
		switch( c ){
			case KEY_SPACE:
				// nice convention: reset the model matrix
				// UIState::ResetModelTransform();
			{
				GlutWindow::zoom_to z = { -2, -2, 2, 2 };
				(*global)(z);
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

	static void
	Draw( void )
	{
		{
			std::lock_guard<std::mutex> lock(global->m_mutex);
			glDrawPixels( global->m_region.width, global->m_region.height,
						 GL_RGB, GL_UNSIGNED_BYTE, &global->m_displayBuffer[0]);
		}

		glutSwapBuffers();
	}

	static void
	Mouse( int button, int state, int x, int y )
	{
		if( state !=0 ) return;
		switch( button ){

			case GLUT_LEFT_BUTTON:
			{
				Region & r = global->m_region;
				double cx = r.x0 + x * r.dx;
				double cy = r.y0 + (r.height-y) * r.dy;

				r.dx*=SCALE;
				r.dy*=SCALE;
				r.x0 = cx - r.dx*r.width*0.5;
				r.y0 = cy - r.dy*r.height*0.5;
				std::cout << "Clicked on " << cx << "," << cy << "\n";
				(*global)(zoom());
				//glutIdleFunc( UIState::Animate() );
				// glutPostRedisplay();
			}
				break;

			case GLUT_RIGHT_BUTTON:
			{
				Region & r = global->m_region;
				double cx = r.x0 + x * r.dx;
				double cy = r.y0 + (r.height-y) * r.dy;

				r.dx/=SCALE;
				r.dy/=SCALE;
				r.x0 = cx - r.dx*r.width*0.5;
				r.y0 = cy - r.dy*r.height*0.5;
				std::cout << "Clicked on " << cx << "," << cy << "\n";
				(*global)(zoom());
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
