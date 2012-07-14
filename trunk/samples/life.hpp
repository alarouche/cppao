#include <active/object.hpp>

const int num_rows=20;
const int num_cols=60;

// Active object representing a display device.

class Display : public active::object<Display>
{
public:
	// List of messages:

	// A cell has updated.
	struct cell_update
	{
		int x, y;
		bool alive;
	};

	// Display the grid now.
	struct redraw { };

	// Implementation:
	void active_method( cell_update&& );
	void active_method( redraw&& );

	Display();
private:
	bool grid[num_cols][num_rows];
	int generation;
};

class Controller;

// Active object representing a cell in the grid.
class Cell : public active::object<Cell>
{
public:

	// List of messages:

	// Notify that a neighbour is connected
	struct add_neighbour { Cell * neighbour; } ;
	void active_method( add_neighbour&& );

	// Notify all neighbours of our status
	struct notify_neighbours { };
	void active_method( notify_neighbours&& );

	// Notification from our neighbours
	struct notification{ bool alive; };
	void active_method( notification&& );

	// Compute our own status
	struct compute { };
	void active_method( compute&& );

	bool is_alive;
	int x,y;	// Position of cell in the grid
	Display * display;
	Controller * controller;
	Cell();
private:
	int alive_neighbours;
	std::vector<Cell*> neighbours;
};

// Active object to control the entire game.
class Controller : public active::object<Controller>
{
public:
	// Messages:

	// Kick off computation
	struct compute { };
	void active_method(compute&&);

	// Cells notify when they have notified all of their neighbours
	struct notification_complete { };
	void active_method( notification_complete&& );

	// Cells notify when they have completed their computation
	struct compute_complete { };
	void active_method( compute_complete&& );

	Controller( active::scheduler & tp, int seed );
private:
	static const int total_cells = num_cols*num_rows;
	Cell cell[num_cols][num_rows];
	Display display;
	int progress;
};
