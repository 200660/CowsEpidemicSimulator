//#include <igraph.h>
#include <fstream>
#include <vector>
#include <map>
#include <limits>
#include <cmath>
#include <cstdio>

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

void animation(const char * path) {
	Gnuplot gp;
	std::vector<std::pair<double, double>> pts1;
	std::vector<std::pair<double, double>> pts2;
	std::vector<std::pair<double, double>>::iterator it;

	try {
		sql::Driver *driver;
		sql::Connection *con;
		sql::Statement *stmt;
		sql::ResultSet *res;
		string pass;

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
		res = stmt->executeQuery("SELECT x, y, tagId from tagi");
		while (res->next()) {
			/* Access column data by alias or column name */
			// cout << res->getDouble("x") << endl;
			// it = pts.begin();
			// pts.insert(it, make_pair(res->getDouble("x"), res->getDouble("y")));
			// if(pts.size() > 20)
			// {
			// 	pts.pop_back();
			// }
			if (res->getString("tagId") == "2605893136767365736")
			{
				pts1.push_back(make_pair(res->getDouble("x"), res->getDouble("y")));
			}
			// else if (res->getString("tagId") == "2605893145357300288")
			// {
			// 	pts2.push_back(make_pair(res->getDouble("x"), res->getDouble("y")));
			// }

			// gp << "set title \"" + res->getString("tagTime") + "\"\n";
			// gp << "plot '-' with points title 'cubic'\n";
			// gp.send1d(pts);
			// gp.flush();
			// mysleep(10);
		}
		delete res;
		delete stmt;
		delete con;
		gp << "plot '-' with points lc \"red\" notitle\n";//, '-' with points lc \"blue\" notitle\n";
		gp.send1d(pts1);
		//gp.send1d(pts2);
		gp.flush();

	} catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			<< __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

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

	if (argc == 2)
	{
		animation(argv[1]);
		return 0;
	}
	return 1;
}