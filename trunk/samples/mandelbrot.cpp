#include <active_object.hpp>
#include <vector>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/GLUT.h>

#include <iostream>

const double SCALE=0.5;
static const int STEP=200;
static const int THRESHOLD=100;


struct cell
{
	double x,y,x0,y0;
	bool escaped;
	int iterations;
	void iterate()
	{
		if( !escaped )
		{
			double xx = x*x, yy=y*y;
			escaped = xx+yy>4.0;
			if( !escaped )
			{
				double x1 = xx - yy + x0;
				double y1 = 2*x*y + y0;
				x=x1;
				y=y1;
				++iterations;
			}
		}
	}
	cell(double x, double y) : 
		x(x), y(y), x0(x), y0(y), escaped(false), iterations(0)
	{
	}
};

struct region
{
	double x0, y0, dx, dy;
	int width, height;
	int offset_x, offset_y;
};

struct tile : public active::shared<tile>
{
	std::vector<cell> m_cells;
	region m_region;
	int m_black_cells;
	bool m_out_of_date;
	
	ACTIVE_METHOD( region )
	{
		std::vector<cell> cells;
		cells.reserve( region.width * region.height );
		double x,y;
		int i,j;
		for(j=0,y=region.y0; j<region.height; ++j,y+=region.dy)
			for(i=0,x=region.x0; i<region.width; ++i,x+=region.dx)
				cells.push_back( cell(x,y) );
		m_cells.swap(cells);
		m_region = region;
		m_black_cells=region.width * region.height;
		m_out_of_date=false;
	}
	
	struct iteration_done 
	{ 
		// region reg;
		int completed;
		tile::ptr tile;
	};
	
	// Iterate a number of times
	struct iterate
	{
		int times;
		active::sink<iteration_done> * finished;
	};
	
	ACTIVE_METHOD( iterate )
	{
		int old_black_cells = m_black_cells;
		for(auto & cell : m_cells)
			for(int i=0; i<iterate.times && !cell.escaped; ++i)
			{
				cell.iterate();
				if( cell.escaped ) --m_black_cells;
			}
		iteration_done result = { old_black_cells - m_black_cells, shared_from_this() };
		(*iterate.finished)(result);	
	}
};




static const int KEY_SPACE = ' ';
static const int KEY_ESCAPE = '\033';


class GlutWindow : public active::object, public active::sink<tile::iteration_done>
{
public:

	struct zoom_to
	{
		double x0, y0, x1, y1;
	};
	
	region m_region;
	tile::ptr m_tile;
	
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
		
		if(m_tile) m_tile->m_out_of_date = true;
		m_tile = std::make_shared<tile>();
		(*m_tile)(m_region);
		
		tile::iterate iterate = { STEP, this };
		(*m_tile)(iterate);
	}
	struct zoom{};
	
	ACTIVE_METHOD(zoom)
	{
		if(m_tile) m_tile->m_out_of_date = true;
		m_tile = std::make_shared<tile>();
		(*m_tile)(m_region);
		
		tile::iterate iterate = { STEP, this };
		(*m_tile)(iterate);
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
		
	typedef tile::iteration_done iteration_done;
	// int iterations
	
	ACTIVE_METHOD( iteration_done )
	{
		// std::cout << "Iteration complete!\n";
		if( iteration_done.tile->m_out_of_date ) return;	// Ignore this one
		int i=0;
		auto j = iteration_done.tile->m_cells.cbegin();
		for(int y=0; y<iteration_done.tile->m_region.height; ++y)
			for(int x=0; x<iteration_done.tile->m_region.width; ++x)
			{
				if( j->escaped )
				{
					m_displayBuffer[i++] = (j->iterations) % 255;
					m_displayBuffer[i++] = (80+j->iterations) % 255;
					m_displayBuffer[i++] = (160+j->iterations) % 255;
				}
				else
				{
					m_displayBuffer[i++] = 0;
					m_displayBuffer[i++] = 0;
					m_displayBuffer[i++] = 0;
				}
				++j;
			}
		glutPostWindowRedisplay(win);
		//Draw();
		
		if( iteration_done.completed>THRESHOLD )
		{
			std::cout << iteration_done.completed << " left\n";
			tile::iterate iterate = { STEP, this };
			(*m_tile)(iterate);		
		}
		else
		{
			std::cout << "Ready\n";
		}
	}
	
	static GlutWindow * global;
	
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
#if 0
		// std::cout << "Draw\n";
		static GLfloat polygonColor[] = { .25f, .8f, .8f };
		static GLfloat polygonVertices[][2] =
		{ {-.5f,  .5f}, { .5f,  .5f}, { .5f, -.5f}, {-.5f, -.5f} };
		
		// clear out buffer
		glClearColor( 0.f, 0.f, 0.f, 0.f );
		glClear( GL_COLOR_BUFFER_BIT );
		
		// draw primitives
		glColor3fv( polygonColor );
		glBegin( GL_POLYGON );
		glVertex2fv( polygonVertices[0] );
		glVertex2fv( polygonVertices[1] );
		glVertex2fv( polygonVertices[2] );
		glVertex2fv( polygonVertices[3] );
		glEnd();
#endif
		
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
				region & r = global->m_region;
				double cx = r.x0 + x * r.dx;
				double cy = r.y0 + (r.height-y) * r.dy;
				
				r.dx*=SCALE;
				r.dy*=SCALE;
				r.x0 = cx - r.dx*r.width*0.5;
				r.y0 = cy - r.dy*r.height*0.5;
				std::cout << "Clicked on " << x << "," << y << "\n";
				std::cout << "Clicked on " << cx << "," << cy << "\n";
				(*global)(zoom());
				//glutIdleFunc( UIState::Animate() );
				// glutPostRedisplay();
			}
				break;
				
			case GLUT_RIGHT_BUTTON:
				break;
		}
	}	
};

GlutWindow * GlutWindow::global;


int main(int argc, char**argv)
{
	GlutWindow win(argc, argv);
	GlutWindow::zoom_to z = { -2, -2, 2, 2 };
	win(z);
	active::run run;
	win.runMainLoop();	// Must be called from main thread - limitation of GLUT
}
