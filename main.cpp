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

// useful to draw points in different
const map<int, string> col
{
	{0, "red"},
	{1, "blue"},
	{2, "green"},
	{3, "black"},
	{4, "orange"},
	{5, "purple"}
};

enum class State
{
	Susceptible,
	Infectious,
	Removed,
	Recovered
};

using Point = pair<double, double>;
using Distribution = uniform_int_distribution<default_random_engine::result_type>;
using GenPair = pair<Distribution, default_random_engine>;
using Generator = std::_Bind_helper<false, Distribution &, std::default_random_engine &>::type;
using BernoulliGen = std::_Bind_helper<false, std::bernoulli_distribution &, std::mt19937 &>::type;

struct Cow
{
	Point position;
	Point target;
	vector<Point> way;
	unsigned int waypoint = 0; // number of current point from the way vector
	unsigned int stayTime = 0;
	State state = State::Susceptible;
	double exposition = 1.0;
	unsigned int infectionTime;
	double infectionTendency = 1.0;
	bool isInRange = false;

	void clear()
	{
		way.clear();
		waypoint = 0;
		state = State::Susceptible;
		exposition = 1.0;
		infectionTendency = 1.0;
		isInRange = false;
	}

	void print()
	{
	 	cout << "position: " << position.first << ", " << position.second << endl \
	 		 << "target: " << target.first << ", " << target.second  << endl \
	 		 << way[waypoint].first << ", " << way[waypoint].second << endl \
	 		 << "waypoint: " << waypoint << endl \
	 		 << "stayTime: " << stayTime << endl << endl;
	}
};

struct Illness
{
	double infectionRate;
	double recoverRate;
	unsigned int infectionRange;
	unsigned int infectionDuration = 172800;

	string print()
	{
		return string(to_string(infectionRate) + " " + to_string(recoverRate) + " " + to_string(infectionRange));
	}
};

struct Clock
{
	unsigned int day = 1;
	unsigned int hours = 6;
	unsigned int minutes = 0;
	unsigned int seconds = 0;

	void tick()
	{
		seconds++;
		if (seconds == 60)
		{
			seconds = 0;
			minutes++;
			if (minutes == 60)
			{
				minutes = 0;
				hours++;
				if (hours == 24)
				{
					hours = 0;
					day++;
				}
			}
		}
	}
	
	bool isSleepTime()
	{
		return (hours >= 20 or hours < 6);
	}

	string to_string()
	{
		return string(std::to_string(day) + ":" + std::to_string(hours) + ":" + string((minutes < 10) ? "0" : "") + std::to_string(minutes)
					+ ":" + ((seconds < 10) ? "0" : "") + std::to_string(seconds) + (isSleepTime() ? " - sleep" : " - day"));
	}
};

// algorithm based on Bresenham's line algorithm
void line(const Point & start, const Point & target, vector<Point>& way)
{
	double x = start.first;
	double y = start.second;
	double xDelta = abs(target.first - start.first);
	double yDelta = abs(target.second - start.second);
	auto deltasDiff = abs(yDelta - xDelta) * (-2);

	way.push_back(start);

	// 0X is main
	if (xDelta > yDelta)
	{
		auto bi = yDelta * 2;
		auto delta = bi - xDelta;
		while (x != target.first)
		{
			if (delta >= 0)
			{
				x += start.first < target.first ? 1 : -1;
				y += start.second < target.second ? 1 : -1;
				delta += deltasDiff;
			}
			else
			{
				x += start.first < target.first ? 1 : -1;
				delta += bi;
			}
			way.push_back(make_pair(x, y));
		}
	}
	// 0Y is main
	else
	{
		auto bi = xDelta * 2;
		auto delta = bi - yDelta;
		while (y != target.second)
		{
			if (delta >= 0)
			{
				x += start.first < target.first ? 1 : -1;
				y += start.second < target.second ? 1 : -1;
				delta += deltasDiff;
			}
			else
			{
				y += start.second < target.second ? 1 : -1;
				delta += bi;
			}
			way.push_back(make_pair(x, y));
		}
	}
}

void initialize(vector<Cow> & cows, Generator & xGen, Generator & yGen)
{
	for (unsigned i = 0; i < cows.size(); i++)
	{
		cows[i].position.first = xGen();
		cows[i].position.second = yGen();
		//cout << i << " position: " << cows[i].position.first << ", " << cows[i].position.second << endl;
		cows[i].target.first = xGen();
		cows[i].target.second = yGen();
		line(cows[i].position, cows[i].target, cows[i].way);
		// cows[i].print();
	}
}

size_t get_seed()
{
	random_device entropy;
	return entropy();
}

void moveCows(vector<Cow> & cows, Generator & xGen, Generator & yGen,
				Generator & speedGen, Generator & stayGen/*, string & command*/)
{


	for (unsigned i = 0; i < cows.size(); i++)
	{
		if (cows[i].state == State::Removed)
		{
			continue;
		}

		if (cows[i].target == cows[i].position)
		{
			cows[i].target.first = xGen();
			cows[i].target.second = yGen();
			cows[i].way.clear();
			cows[i].waypoint = 0;
			line(cows[i].position, cows[i].target, cows[i].way);
			cows[i].stayTime = stayGen();
			//cout << i << " target: " << cows[i].target.first << ", " << cows[i].target.second << endl;
		}

		if (cows[i].stayTime == 0)
		{
			cows[i].waypoint += speedGen();
			if (cows[i].waypoint + 1 >= cows[i].way.size())
			{
				// check first if point is free
				cows[i].position.first = cows[i].target.first;
				cows[i].position.second = cows[i].target.second;
				cows[i].waypoint = cows[i].way.size() - 1;
			}
			else
			{
				// check first if point is free
				cows[i].position.first = cows[i].way[cows[i].waypoint].first;
				cows[i].position.second = cows[i].way[cows[i].waypoint].second;
			}
		}
		else
		{
			cows[i].stayTime -= 1;
		}
		// cows[i].print();
		// command = command + " '-' with point lc \"" + col.at(i) + "\" notitle,";
	}
}

double getDistance(Cow & cow1, Cow & cow2)
{
	return sqrt(pow((cow1.position.first - cow2.position.first), 2) + pow((cow1.position.second - cow2.position.second), 2));
}

bool spreadDisease(vector<Cow> & cows, mt19937 & probabilityGen, Illness & illness/*, Clock & clock, int experimentNum*/)
{
	bool areAllRemoved = true;

	for (unsigned i = 0; i < cows.size(); i++)
	{
		// cout << "cow#" << i << "" << endl;
		// cout << 1 << " ";
		if (cows[i].state == State::Infectious)
		{
			for (unsigned j = 0; j < cows.size(); j++)
			{
				auto distance = getDistance(cows[i], cows[j]);
				// cout << i << " <-> " << j << " = " << distance << endl;
				if ((cows[j].state == State::Susceptible) and (distance <= illness.infectionRange))
				{
					//cout << i << " <-> " << j << " = " << distance << endl;
					cows[j].isInRange = true;
					bernoulli_distribution probabilityDist(illness.infectionRate/* * cows[j].exposition*/);
					//auto prob = double(probabilityGen())/10000.0;
					//cout << i << " - " << j << " -> " << prob << endl;
					if (probabilityDist(probabilityGen))
					{
						//cout << clock.to_string() << ": " << i << " - " << j << " -> " << prob << endl;
						cows[j].state = State::Infectious;
						cows[j].infectionTime = illness.infectionDuration;
					}
				}
			}

			// cout << 2 << " ";
			cows[i].infectionTime -= 1;
			if (cows[i].infectionTime == 0) 
			{
				// cout << 3 << " ";
				if (double(probabilityGen())/10000.0 <= illness.recoverRate)
				{
					// cout << 4 << " ";
					cows[i].state = State::Recovered;
				}
				else
				{
					// cout << 5 << " ";
					cows[i].state = State::Removed;
				}
			}
			else
			{
				areAllRemoved = false;
			}
			
			// cout << endl;
		}
		else if (cows[i].state == State::Susceptible)
		{
			// cout << "not Susceptible" << endl;
			areAllRemoved = false;
		}
	}

	// if (experimentNum > 1)
	// {
	// 	for (unsigned i = 0; i < cows.size(); i++)
	// 	{
	// 		if (cows[i].isInRange)
	// 		{
	// 			cows[i].exposition += 0.1;
	// 			cows[i].isInRange = false;
	// 		}
	// 		else
	// 		{
	// 			cows[i].exposition = 1.0;
	// 		}
	// 	}
	// }

	return areAllRemoved;
}

void experiment(const char * inputFile, int duration /*number corresponds to real days*/, int experimentNum)
{
	ifstream data;
	data.open(inputFile);
	if(data.good() != true)
	{
		cerr << "Data file is missing!" << endl;
		return;
	}

	ofstream output;
	string outputName = "output" + to_string(experimentNum);
	output.open(outputName.c_str(), ios::out);
	if(output.good() != true)
	{
		cerr << "Output file is missing!" << endl;
		return;
	}

	// do
	// {
		string tmp;
		data >> tmp;
		unsigned int cowsNumber = stoi(tmp);
		vector<Cow> cows(cowsNumber);

		Illness illness;
		data >> tmp;
		illness.infectionRate = stod(tmp);
		data >> tmp;
		illness.recoverRate = stod(tmp);
		data >> tmp;
		illness.infectionRange = stod(tmp);
		tmp = to_string(cowsNumber) + string(" ") + illness.print();


		Distribution xDist(0, 4000);
		default_random_engine xEngine;
		// default_random_engine::result_type const xseed = get_seed();
		// xEngine.seed(xseed);
		auto xGen = bind(xDist, xEngine);

		Distribution yDist(0, 1750);
		default_random_engine yEngine;
		// default_random_engine::result_type const yseed = get_seed();
		// yEngine.seed(yseed);
		auto yGen = bind(yDist, yEngine);

		Distribution speedDist(11, 23);
		default_random_engine speedEngine;
		// default_random_engine::result_type const speedseed = get_seed();
		// speedEngine.seed(speedseed);
		auto speedGen = bind(speedDist, speedEngine);

		Distribution stayDist(0, 1800);
		default_random_engine stayEngine;
		// default_random_engine::result_type const stayseed = get_seed();
		// stayEngine.seed(stayseed);
		auto stayGen = bind(stayDist, stayEngine);

		random_device probabilityRD;
		mt19937 probabilityEngine(probabilityRD());
		//default_random_engine::result_type const probabilityseed = get_seed();
		//probabilityEngine.seed(probabilityseed);
		//auto probabilityGen = bind(probabilityDist, probabilityEngine);

		// Gnuplot gp;
		// gp << "set xrange [0:4000]\n";
		// gp << "set yrange [0:1750]\n";

		unsigned int rCows = 0;
		unsigned int durationInSec = duration * 24 * 60 * 60;
		unsigned int stopTime = 0;
		for (size_t i = 0; i < 1; i++)
		{
			stopTime += durationInSec;
			initialize(cows, xGen, yGen);
			cows[0].state = State::Infectious;
			cows[0].infectionTime = illness.infectionDuration;
			Clock clock;
			for (unsigned int j = 0; j < durationInSec; j++)
			{
				// cout << j << " ----------------------------------- " << endl;
				//string command = "plot";

				if (!clock.isSleepTime())
				{
					moveCows(cows, xGen, yGen, speedGen, stayGen/*, command*/);
				}

				if (spreadDisease(cows, probabilityEngine, illness/*, clock, experimentNum*/))
				{
					stopTime = stopTime - durationInSec + j;
					break;
				}

				//cout << command + "\n";
				// gp << "set title \"" + clock.to_string() + "\"\n";
				// gp << command + "\n";
				// for (size_t i = 0; i < cows.size(); i++)
				// {
				// 	vector<pair<double, double>> pts;
				// 	pts.push_back(make_pair(double(cows[i].position.first), double(cows[i].position.second)));
				// 	gp.send1d(pts);
				// }
				// gp.flush();
				// mysleep(2);
				clock.tick();
			}

			unsigned int r = 0;
			for (size_t j = 0; j < cows.size(); j++)
			{
				if (cows[j].state == State::Recovered or cows[j].state == State::Removed)
				{
					r++;
				}
				cows[j].clear();
			}
			rCows += r;
		}

		tmp += " " + to_string(double(stopTime) / (30.0 * 30.0 * 3.0)) + " " + to_string(double(rCows)/3.0);
		cout << tmp << endl;
		//output << tmp << endl;
	// } while (!data.eof());

	data.close();
	output.close();
}

void animation(const char * path)
{
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

int main(int argc, char **argv)
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

	try
	{
		int c;
		unsigned int experimentNum;
		unsigned int duration = 0;
		char * inputFile;
		while ((c = getopt(argc, argv, "e:f:d:")) != -1)
		switch (c)
		{
		case 'e':
			experimentNum = stoi(optarg);
			break;
		case 'd':
			duration = stoi(optarg);
			break;
		case 'f':
			inputFile = optarg;
			break;
		default:
			break;
		}

		experiment(inputFile, duration, experimentNum);
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	// Point a(2000, 800);
	// Point b(1900, 790);
	// vector<Point> way;
	// line(a,b,way);

	// vector<pair<double, double>> pts;
	// for (size_t i = 0; i < way.size(); i++)
	// {
	// 	pts.push_back(make_pair(way[i].first, way[i].second));
	// }

	// Gnuplot gp;
	// gp << "set xrange [0:4000]\n";
	// gp << "set yrange [0:1750]\n";
	// gp << "plot '-' with points lc \"red\" notitle\n";
	// gp.send1d(pts);
	// gp.flush();

	return 0;
}