//#include <igraph.h>
#include <fstream>
#include <vector>
#include <map>
#include <limits>
#include <cmath>
#include <cstdio>
#include <random>

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

// Warn about use of deprecated functions.
#define GNUPLOT_DEPRECATE_WARN
#include "gnuplot-iostream.h"

#include <unistd.h>
inline void mysleep(unsigned millis) {
	::usleep(millis * 1000);
}

using namespace std;

const map<int, string> col
{
	{0, "red"},
	{1, "blue"},
	{2, "green"},
	{3, "black"}
};

class Point
{
public:
	int x;
	int y;

	Point(){};

	Point(int x, int y)
	{
		this->x = x;
		this->y = y;
	}

	bool isSame(Point & p)
	{
		return this->x == p.x and this->y == p.y;
	}
};

class Cow
{
public:
	Point position;
	Point target;
	vector<Point> way;
	unsigned int waypoint = 0; // number of point form from way list
};

void line(Point & start, Point & target, vector<Point>& way) 
{
	double deltaX = target.x - start.x;
	double deltaY = target.y - start.y;
	double growth = 0;
	double deltaGrowth = (deltaX != 0 ? abs(deltaY / deltaX) : 1);
	auto x = start.x;
	auto y = start.y;
	way.push_back(start);
	do
	{
		x += (start.x < target.x ? 1 : -1);
		growth += deltaGrowth;
		if (growth >= 0.5)
		{
			y += (start.y < target.y ? 1 : -1);
			--growth;
		}

		Point temp(x,y);
		way.push_back(temp);
	} while (x != target.x);
}

size_t get_seed() {
    random_device entropy;
    return entropy();
}

void experiment(const int cowsNumber, int duration /*in seconds*/)
{
	Gnuplot gp;
	gp << "set xrange [0:4000]\n";
	gp << "set yrange [0:1750]\n";
	vector<Cow> cows(cowsNumber);
	uniform_int_distribution<default_random_engine::result_type> xDist(0, 4000);
	uniform_int_distribution<default_random_engine::result_type> yDist(0, 1750);
	uniform_int_distribution<default_random_engine::result_type> speedDist(11, 23);

	default_random_engine xEngine;
	default_random_engine yEngine;
	default_random_engine speedEngine;

	default_random_engine::result_type const xSeed = get_seed();
	default_random_engine::result_type const ySeed = get_seed();
	default_random_engine::result_type const speedSeed = get_seed();

	xEngine.seed(xSeed);
	yEngine.seed(ySeed);
	speedEngine.seed(speedSeed);

	auto xGen = bind(xDist, xEngine);
	auto yGen = bind(yDist, yEngine);
	auto speedGen = bind(speedDist, speedEngine);

	for (auto i = 0; i < cowsNumber; i++)
	{
		cows[i].position.x = xGen();
		cows[i].position.y = yGen();
		//cout << i << " position: " << cows[i].position.x << ", " << cows[i].position.y << endl;
		cows[i].target.x = cows[i].position.x;
		cows[i].target.y = cows[i].position.y;
	}

	for (int j = 0; j < duration; j++)
	{
		string command = "plot";
		for (int i = 0; i < cowsNumber; i++)
		{
			if (cows[i].target.isSame(cows[i].position))
			{
				cows[i].target.x = xGen();
				cows[i].target.y = yGen();
				cows[i].way.clear();
				cows[i].waypoint = 0;
				line(cows[i].position, cows[i].target, cows[i].way);
				//cout << i << " target: " << cows[i].target.x << ", " << cows[i].target.y << endl;
			}

			unsigned int speed = speedGen();
			if (i == 0)
			{
				cout << " 0 speed: " << speed << endl;
			}
			cows[i].waypoint += speed;
			if (cows[i].waypoint + 1 >= cows[i].way.size())
			{
				// check first if point is free
				cows[i].position.x = cows[i].target.x;
				cows[i].position.y = cows[i].target.y;
			}
			else
			{
				// check first if point is free
				cows[i].position.x = cows[i].way[cows[i].waypoint].x;
				cows[i].position.y = cows[i].way[cows[i].waypoint].y;
			}
			command = command + " '-' with point lc \"" + col.at(i) + "\" notitle,";
		}

		gp << command + "\n";
		for (int i = 0; i < cowsNumber; i++)
		{
			vector<pair<double, double>> pts;
			pts.push_back(make_pair(double(cows[i].position.x), double(cows[i].position.y)));
			gp.send1d(pts);
		}
		gp.flush();
		mysleep(60);
	}
}

void animation(const char * path) {
	Gnuplot gp;
	vector<pair<double, double>> pts1;
	vector<pair<double, double>> pts2;
	vector<pair<double, double>>::iterator it;

	try {
		sql::Driver *driver;
		sql::Connection *con;
		sql::Statement *stmt;
		sql::ResultSet *res;
		string pass;
		// double sum = 0.0;
		// double max = 0.0;
		// double min = 17.0;
		// double prevX = 0.0;
		// double prevY = 0.0;
		// double count = 0;
		// bool isFirst = true;

		gp << "set xrange [-3000:5200]\n";
		gp << "set yrange [-2600:1750]\n";

		ifstream passfile;
		passfile.open(path);
		if(passfile.good() == true)
		{
			passfile >> pass;
			passfile.close();
		}

		/* Create a connection */
		driver = get_driver_instance();
		con = driver->connect("tcp://127.0.0.1:3306", "root", sql::SQLString(pass));
		/* Connect to the MySQL test database */
		con->setSchema("rtls");

		stmt = con->createStatement();
		res = stmt->executeQuery("SELECT x, y from tagi where tagId=2605893136767365736");

		while (res->next()) {
			/* Access column data by alias or column name */
			// cout << res->getDouble("x") << endl;
			// it = pts.begin();
			// pts.insert(it, make_pair(res->getDouble("x"), res->getDouble("y")));
			// if(pts.size() > 20)
			// {
			// 	pts.pop_back();
			// }
			// if (res->getString("tagId") == "2605893136767365736")
			// {
			//	pts1.push_back(make_pair(res->getDouble("x"), res->getDouble("y")));
			// }
			// else if (res->getString("tagId") == "2605893145357300288")
			// {
			// 	pts2.push_back(make_pair(res->getDouble("x"), res->getDouble("y")));
			// }

			///Rysowanie krok po kroku/////////////////////////////////
			// gp << "set title \"" + res->getString("tagTime") + "\"\n";
			// gp << "plot '-' with line title 'cubic'\n";
			// gp.send1d(pts);
			// gp.flush();
			// mysleep(10);
			
			///////////////////////////////////////////////////////////
			// if (!isFirst)
			// {
			// 	auto x = abs(prevX - res->getDouble("x"));
			// 	auto y = abs(prevY - res->getDouble("y"));
			// 	auto sq = sqrt(x*x+y*y);
			// 	if (sq > max) max = sq;
			// 	if (sq < min) min = sq;
			// 	sum += sq;
			// 	count++;
			// }
			// else
			// {
			// 	isFirst = false;
			// }
			// prevX = res->getDouble("x");
			// prevY = res->getDouble("y");
		}
		delete res;
		delete stmt;
		delete con;

		// cout << "avg: " << sum/count << endl;
		// cout << "min: " << min << endl;
		// cout << "max: " << max << endl;

		// gp << "plot '-' with line lc \"red\" notitle\n";//, '-' with line lc \"blue\" notitle\n";
		// gp.send1d(pts1);
		// gp.send1d(pts2);
		// gp.flush();

	} catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			<< __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

	// oryginalny przykład z gnuplot //////////////////////////////////
	// cout << "Press Ctrl-C to quit (closing gnuplot window doesn't quit)." << endl;

	// gp << "set title \"CowsTracker\"\n";
	// //gp << "set s \"CowsTracker\"\n";
	// gp << "set yrange [-1:1]\n";

	// const int N = 1000;
	// vector<double> pts(N);

	// double theta = 0;
	// while(1) {
	// 	for(int i=0; i<N; i++) {
	// 		double alpha = (double(i)/N-0.5) * 10;
	// 		pts[i] = sin(alpha*8.0 + theta) * exp(-alpha*alpha/2.0);
	// 	}

	// 	cout << endl << gp.binFmt1d(pts, "array") << endl;
	// 	gp << "plot '-' binary" << gp.binFmt1d(pts, "array") << "with lines notitle\n";
	// 	gp.sendBinary1d(pts);
	// 	gp.flush();

	// 	theta += 0.2;
	// 	mysleep(1000);
	// }
}

int main(/*int argc, char **argv*/)
{
	// igraph_t graph;
	// igraph_vector_t v;
	// igraph_vector_t result;
	// igraph_real_t edges[] = { 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8,
	// 						0,10, 0,11, 0,12, 0,13, 0,17, 0,19, 0,21, 0,31,
	// 						1, 2, 1, 3, 1, 7, 1,13, 1,17, 1,19, 1,21, 1,30,
	// 						2, 3, 2, 7, 2,27, 2,28, 2,32, 2, 9, 2, 8, 2,13,
	// 						3, 7, 3,12, 3,13, 4, 6, 4,10, 5, 6, 5,10, 5,16,
	// 						6,16, 8,30, 8,32, 8,33, 9,33,13,33,14,32,14,33,
	// 						15,32,15,33,18,32,18,33,19,33,20,32,20,33,
	// 						22,32,22,33,23,25,23,27,23,32,23,33,23,29,
	// 						24,25,24,27,24,31,25,31,26,29,26,33,27,33,
	// 						28,31,28,33,29,32,29,33,30,32,30,33,31,32,31,33,
	// 						32,33};

	// igraph_vector_view(&v, edges, sizeof(edges)/sizeof(double));
	// igraph_create(&graph, &v, 0, IGRAPH_UNDIRECTED);

	// igraph_vector_init(&result, 0);

	// igraph_degree(&graph, &result, igraph_vss_all(), IGRAPH_ALL,
	// 				IGRAPH_LOOPS);
	// printf("Maximum degree is	 %10i, vertex %2i.\n",
	// 		(int)igraph_vector_max(&result), (int)igraph_vector_which_max(&result));

	// igraph_closeness(&graph, &result, igraph_vss_all(), IGRAPH_ALL, /*weights=*/ 0, true);
	// printf("Maximum closeness is	 %10f, vertex %2i.\n", (double)igraph_vector_max(&result), (int)igraph_vector_which_max(&result));

	// igraph_betweenness(&graph, &result, igraph_vss_all(),
	// 					IGRAPH_UNDIRECTED, /*weights=*/ 0, /*nobigint=*/ 1);
	// printf("Maximum betweenness is %10f, vertex %2i.\n",
	// 		(double)igraph_vector_max(&result), (int)igraph_vector_which_max(&result));

	// igraph_vector_destroy(&result);
	// igraph_destroy(&graph);

	// if (argc == 2)
	// {
	// 	animation(argv[1]);
	// 	return 0;
	// }

	experiment(4, 600);
	return 0;
}