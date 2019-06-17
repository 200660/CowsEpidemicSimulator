#include <fstream>
#include <vector>
#include <map>
#include <cmath>

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;
using Heatmap = map<uint32_t, map<uint32_t, uint64_t>>;

using ActivityMap = map<uint32_t, map<string, double>>; // <godzina, <tagID, pair (przebyta odległość, ilość kroków)>>

struct Lasts
{
	uint32_t x;
	uint32_t y;
	int hour;
	int day;
};

using TagsLasts = map<string, Lasts>; // <tagID, pair (last_x, last_y)>

const vector<string> tagi = {
	"2605892041544634063",
	"2605892063019470543",
	"2605893141062333093",
	"2605893145357300308",
	"2605893166832136808",
	"2605893145357300288",
	"2605893153947234920",
	"2605893136767365736",
	"2605893141062333012",
	"2605893141062333032",
	"2605893141062333113"};

void initHeatmap(Heatmap &heatmap)
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
	initHeatmap(heatmap);

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
		res = stmt->executeQuery("SELECT x, y FROM tagi ORDER BY tagTime");
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

void initActivityMap(ActivityMap &activityMap, TagsLasts &lasts)
{
	for (uint32_t h = 0; h <= 23; h++)
	{
		for (const auto &tag : tagi)
		{
			activityMap[h][tag] = 0;
		}
	}

	for (const auto &tag : tagi)
	{
		lasts[tag].x = 0;
		lasts[tag].y = 0;
		lasts[tag].hour = -1;
		lasts[tag].day = -1;
	}
}

void printActivities(ActivityMap &activityMap)
{
	ofstream file;
	file.open("activities.csv", ofstream::out);
	if (file.good())
	{
		file << "godzina,średni dystans" << endl;
		for (auto &activity : activityMap)
		{
			double average = 0.0;
			for (auto &tag : activity.second)
			{
				average += tag.second;
			}
			average /= double(tagi.size()); // przez liczbę krów
			if (activity.first >= 11 and activity.first <= 18)
			{
				average /= 21; // przez liczbę dni
			}
			else
			{
				average /= 20; // przez liczbę dni
			}
			average /= 100000; // zamiana na kilometry
			file << activity.first << "," << fixed << average << endl;
		}
	}
	file.close();
}

void analyzeHoursActivity(const char *pathToPassfile)
{
	ActivityMap activityMap;
	TagsLasts lasts;
	initActivityMap(activityMap, lasts);

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
		res = stmt->executeQuery("SELECT x, y, tagId, HOUR(tagTime) as hour, DAY(tagTime) as day FROM tagi ORDER BY tagTime");
		while (res->next())
		{
			int x = res->getInt("x");
			int y = res->getInt("y");
			if (x < 0 or y < 0)
			{
				continue;
			}
			string tagId = res->getString("tagId");
			int hour = res->getInt("hour");
			int day = res->getInt("day");

			if (lasts[tagId].hour == hour and lasts[tagId].day == day)
			{
				double diffX = double(x) - double(lasts[tagId].x);
				double diffY = double(y) - double(lasts[tagId].y);
				double dist = sqrt(pow(diffX, 2.0) + pow(diffY, 2.0));
				activityMap[hour][tagId] += dist;
				lasts[tagId].x = x;
				lasts[tagId].y = y;
			}
			else
			{
				if (lasts[tagId].hour != hour)
				{
					lasts[tagId].hour = hour;
				}
				if (lasts[tagId].day != day)
				{
					lasts[tagId].day = day;
				}
				lasts[tagId].x = x;
				lasts[tagId].y = y;
			}
		}
		delete res;
		delete stmt;
		delete con;

		printActivities(activityMap);
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
		//prepareHeatmap(argv[1]);
		analyzeHoursActivity(argv[1]);
		return 0;
	}
	return 1;
}