#include <fstream>
#include <vector>
#include <map>

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;
using Heatmap = map<uint32_t, map<uint32_t, uint64_t>>;

void init(Heatmap &heatmap)
{
	for (uint32_t x = 100; x <= 4000; x += 100)
	{
		for (uint32_t y = 100; y <= 2100; y += 100)
		{
			heatmap[x][y] = 0;
		}
	}
}

void assign(Heatmap &heatmap, int x, int y)
{
	int xTemp;
	int yTemp;
	for (int i = 100; i <= 4000; i += 100)
	{
		if (x <= i and x >= 0)
		{
			xTemp = i;
			break;
		}
	}
	for (int i = 100; i <= 2100; i += 100)
	{
		if (y <= i and y >= 0)
		{
			yTemp = i;
			break;
		}
	}
	heatmap[xTemp][yTemp] += 1;
}

void printHeatmap(Heatmap &heatmap)
{
	ofstream file;
	file.open("heatmap.csv", ofstream::out);
	if (file.good())
	{
		string names = "";
		for (uint32_t x = 100; x <= 4000; x += 100)
		{
			names += to_string(x) + ",";
		}
		names.pop_back();
		file << names << endl;
		for (uint32_t y = 2100; y >= 100; y -= 100)
		{
			string line = to_string(y);
			for (uint32_t x = 100; x <= 4000; x += 100)
			{
				line += "," + to_string(heatmap[x][y]);
			}
			file << line << endl;
		}
	}
	file.close();
}

void prepareHeatmap(const char *pathToPassfile)
{
	Heatmap heatmap;
	init(heatmap);

	try
	{
		sql::Driver *driver;
		sql::Connection *con;
		sql::Statement *stmt;
		sql::ResultSet *res;
		string pass;

		ifstream passfile;
		passfile.open(pathToPassfile);
		if (passfile.good() == true)
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
		res = stmt->executeQuery("SELECT x, y from tagi");
		while (res->next())
		{
			assign(heatmap, res->getInt("x"), res->getInt("y"));
		}
		delete res;
		delete stmt;
		delete con;

		printHeatmap(heatmap);
	}
	catch (sql::SQLException &e)
	{
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
			 << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

int main(int argc, char **argv)
{
	if (argc == 2)
	{
		prepareHeatmap(argv[1]);
		return 0;
	}
	return 1;
}