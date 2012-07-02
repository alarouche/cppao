#include <active_object.hpp>
#include <vector>

namespace mb
{
	const double SCALE=0.25;
	const int STEP=250;
	const int THRESHOLD=1;
	const int MAX_ITERATIONS=100000;
	const int TILES=8;

	// An area on the screen
	struct Region
	{
		double x0, y0, dx, dy;
		int width, height;
		int offset_x, offset_y;

		int framebuffer_offset(int x, int y, int bpp=3) const;
		int framebuffer_offset(double x, double y, int bpp=3) const;
	};

	// An individual point or pixel
	struct Cell
	{
		Cell();
		void iterate(double x0, double y0);
		int iterations() const;
		bool escaped() const;
	private:
		double x,y;
		bool m_escaped;
		int m_iterations;
	};

	// Active object to compute a region
	struct ComputeRegion : public active::shared<ComputeRegion>
	{
		struct Ready;

		// 1) Construct the region
		ACTIVE_METHOD( Region );

		// 2) Iterate a number of times
		struct Iterate
		{
			int times;
			active::sink<Ready>::sp finished;
		};
		ACTIVE_METHOD( Iterate );

		// 3) Message sent when iteration finished.
		struct Ready
		{
			Region region;
			std::vector<int> iterations;
			int completed;
			int min_iterations, max_iterations;
			int escapedCount;
			int totalIterations;
		};

	private:
		std::vector<Cell> m_cells;
		Region m_region;
		int m_totalIterations;
	};

	// Active object to convert a iteration counts into colours.
	struct RenderRegion :
		public active::shared<RenderRegion>,
		public active::sink<ComputeRegion::Ready>
	{
		// 1) Set up the recipient of the result.
		struct Ready;
		typedef active::sink<Ready>::sp SetNotifier;

		ACTIVE_METHOD( SetNotifier );

		// 2) Notify that computation is complete
		typedef ComputeRegion::Ready ComputeReady;

		ACTIVE_METHOD( ComputeReady );

		// 3) Send the Ready message to the recipient.
		struct Ready
		{
			Region reg;
			std::vector<unsigned char> buffer;
			int completed;
			int minIterations;
			int escapedCount;
			int totalIterations;
		};

	private:
		active::sink<Ready>::wp m_notifier;
	};

	// Active object to manage the computation and rendering of the entire view.
	struct View :
		public active::shared<View>,
		public active::sink<RenderRegion::Ready>
	{
		// 0) Construct
		View();

		// 1) Construct the view with a region and a call-back to update.
		struct Update;

		struct Start
		{
			Region region;
			active::sink<Update>::sp updater;	// Whom to notify when contents have changed
		};

		ACTIVE_METHOD( Start );

		// 2) Construct, specifying an initial framebuffer.
		struct StartFromOtherView
		{
			Start newSettings;
			std::vector<unsigned char> newBuffer;
			int minIterations, maxIterations;
		};

		ACTIVE_METHOD( StartFromOtherView );

		// 3) Notify someone that the framebuffer has changed.
		struct Update
		{
			int width;
			int height;
			std::vector<unsigned char> buffer;
		};

		// 4) A Region has finished rendering.
		typedef RenderRegion::Ready RenderReady;
		ACTIVE_METHOD( RenderReady );

		// 5) Destroy this view and start another view
		struct ZoomTo
		{
			Start newSettings;
			ptr newView;
		};
		ACTIVE_METHOD( ZoomTo );

		// 6) Stop all processing (to exit application)
		struct Stop { };

		ACTIVE_METHOD( Stop );

	private:
		Region m_region;
		bool m_dismissed;
		std::vector<unsigned char> m_displayBuffer;
		active::sink<Update>::wp m_update;
		int m_minIterations, m_maxIterations;
		int m_tilesRemaining;
		int m_maxUnescapedIterations;

		struct Tile
		{
			ComputeRegion::ptr compute;
			RenderRegion::ptr render;
		} m_tiles[TILES][TILES];
	};

	// Active object to render the window using GLUT.
	class GlutWindow :
		public active::shared<GlutWindow>,
		public active::sink<View::Update>
	{
	public:
		// 0) Construct
		GlutWindow(int argc, char**argv);

		// 1) Navigate to a new region
		struct NavigateTo
		{
			double x0, y0, x1, y1;
		};
		ACTIVE_METHOD( NavigateTo );

		struct Navigate {};
		ACTIVE_METHOD(Navigate);

		// 2) Update the display buffer
		typedef View::Update ViewUpdate;
		ACTIVE_METHOD( ViewUpdate );

		void runMainLoop();

	private:
		struct exit_main_loop { };
		std::shared_ptr<View> view;
		Region m_region;

		int m_window;
		static GlutWindow * global;

		// Because GLUT itself is not threadsafe and insists on running
		// in the main thread...
		std::mutex m_mutex;
		std::vector<unsigned char> m_displayBuffer;

		void OnIdle();
		static void Idle();
		void OnReshape(int width, int height);
		static void Reshape(int width, int height);
		static void Keyboard( unsigned char c, int x, int y );
		void OnKeyboard(unsigned char c, int x, int y );
		static void Draw( void );
		void OnDraw();
		static void Mouse( int button, int state, int x, int y );
		void OnMouse(int button, int state, int x, int y);
	};
}
