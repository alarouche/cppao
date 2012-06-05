#include <active_object.hpp>

#include <iostream>
#include <vector>
#include <random>
#include "life.hpp"

/* This demo implements Conway's Game of Life.
 * As an exercise in perversity, everything has been implemented as Active Methods.
 */


Controller::Controller( active::pool & tp, int seed )
{
	std::mt19937 rng;
	rng.seed(seed);
	std::uniform_int_distribution<int> uint_dist(0,1);
	
	set_scheduler(tp);
	display.set_scheduler(tp);
	for(int x=0; x<num_cols; ++x)
		for(int y=0; y<num_rows; ++y)
		{
			cell[x][y].x = x;
			cell[x][y].y = y;
			cell[x][y].display = &display;
			cell[x][y].controller = this;
			cell[x][y].set_scheduler(tp);
			
			// Set up cell neighbours.
			Cell::add_neighbour an;
			int x0=(x-1)%num_cols, x1=(x+1)%num_cols, y0=(y-1)%num_rows, y1=(y+1)%num_rows;

			if( x0<0 ) x0+=num_cols;	// Thank you % for not being intuituive...
			if( y0<0 ) y0+=num_rows;

			an.neighbour = &cell[x0][y0];
			cell[x][y](an);
			an.neighbour = &cell[x0][y];
			cell[x][y](an);
			an.neighbour = &cell[x0][y1];
			cell[x][y](an);
			an.neighbour = &cell[x][y0];
			cell[x][y](an);
			an.neighbour = &cell[x][y1];
			cell[x][y](an);
			an.neighbour = &cell[x1][y0];
			cell[x][y](an);
			an.neighbour = &cell[x1][y];
			cell[x][y](an);
			an.neighbour = &cell[x1][y1];
			cell[x][y](an);	
			
			cell[x][y].is_alive = (bool)uint_dist(rng);
		}
}	

void Controller::ACTIVE_IMPL(compute)
{
	progress = total_cells;
	for(int x=0; x<num_cols; ++x)
		for(int y=0; y<num_rows; ++y)
			cell[x][y](Cell::notify_neighbours());
}


void Controller::ACTIVE_IMPL( compute_complete )
{
	if( --progress == 0 )
	{
		display(Display::redraw());
		
		// Loop again
		(*this)(compute());
	}
}

void Controller::ACTIVE_IMPL( notification_complete )
{
	if( --progress == 0 )
	{
		progress = total_cells;
		for(int x=0; x<num_cols; ++x)
			for(int y=0; y<num_rows; ++y)
				cell[x][y](Cell::compute());
	}
}

Cell::Cell() : alive_neighbours(0)
{
}

void Cell::ACTIVE_IMPL( add_neighbour )
{
	neighbours.push_back( add_neighbour.neighbour );
}

void Cell::ACTIVE_IMPL( notification )
{
	alive_neighbours += notification.alive;
}

void Cell::ACTIVE_IMPL( notify_neighbours )
{
	notification n = {is_alive};
	
	for( auto i : neighbours )
		(*i)(n);
	
	(*controller)(Controller::notification_complete());
}

void Cell::ACTIVE_IMPL( compute )
{
	if( is_alive && alive_neighbours<2 ) is_alive=false;
	else if( is_alive && alive_neighbours<=3 ) is_alive=true;
	else if( is_alive ) is_alive=false;
	else if( !is_alive && alive_neighbours==3 ) is_alive=true;		
			
	alive_neighbours=0;	// Ready for next generation

	Display::cell_update update = { x,y,is_alive };
	(*display)(update);
	
	(*controller)(Controller::compute_complete());
}

Display::Display() : generation(1)
{
}

void Display::ACTIVE_IMPL( cell_update )
{
	grid[cell_update.x][cell_update.y]=cell_update.alive;
}

void Display::ACTIVE_IMPL( redraw )
{
	std::cout << "\n";
	for(int y=0; y<num_rows; ++y)
	{
		for(int x=0; x<num_cols; ++x)
			std::cout << (grid[x][y]?'#':' ');
		std::cout << '\n';
	}
	std::cout << "Generation " << generation++ << std::flush;
}

int main(int argc, char**argv)
{
	int seed = argc>1 ? atoi(argv[1]) : 101;
	active::pool tp;
	Controller controller(tp, seed);
	controller( Controller::compute() );
	tp.run(4);
	return 0;
}