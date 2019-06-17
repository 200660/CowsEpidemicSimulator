#include <fstream>
#include <vector>
#include <map>
#include <random>
#include <unistd.h>
#include "gnuplot-iostream.h"

// Warn about use of deprecated functions.
#define GNUPLOT_DEPRECATE_WARN

inline void mysleep(unsigned millis)
{
	::usleep(millis * 1000);
}

using namespace std;
using Point = pair<double, double>;
using Distribution = uniform_int_distribution<mt19937::result_type>;
using Generator = _Bind_helper<false, Distribution &, mt19937 &>::type;

// Uncomment flag to draw step by step
// #define SHOULD_DRAW

// useful to draw points in different
const map<int, string> color{
	{0, "red"},
	{1, "blue"},
	{2, "green"},
	{3, "black"},
	{4, "orange"},
	{5, "purple"},
	{5, "yellow"}};

enum class State
{
	Susceptible,
	Exposed,
	Infectious,
	Removed,
	Recovered
};

using SingleResults = map<State, double>;
using Results = map<State, map<double, vector<double>>>;

struct Cow
{
	Point position;
	Point target;
	vector<Point> way;
	unsigned int waypoint = 0; // number of current point from the way vector
	unsigned int stayTime = 0;
	State state = State::Susceptible;
	unsigned int infectionTime;
	unsigned int incubationTime;
	bool isInRange = false;
	unsigned int exposition = 1;

	void clear()
	{
		way.clear();
		waypoint = 0;
		state = State::Susceptible;
		exposition = 1.0;
		isInRange = false;
	}
};

struct Illness
{
	double infectionRate;
	double recoverRate;
	unsigned int infectionRange;
	unsigned int infectionDuration = 86400;

	Illness(double infectionRate, double recoverRate, unsigned int infectionRange) : infectionRate(infectionRate), recoverRate(recoverRate), infectionRange(infectionRange) {}
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
		return string(std::to_string(day) + ":" + std::to_string(hours) + ":" + string((minutes < 10) ? "0" : "") + std::to_string(minutes) + ":" + ((seconds < 10) ? "0" : "") + std::to_string(seconds) + (isSleepTime() ? " - sleep" : " - day"));
	}
};

struct Parameter
{
	double begin;
	double current;
	double end;
	double inc;

	void init()
	{
		current = begin;
	}
	void incr()
	{
		current += (inc == 0 ? 1 : inc);
	}
	bool check()
	{
		return current <= end;
	}
};

enum class Type
{
	Cows,
	InfRate,
	RecRate,
	InfRange
};

struct ExperimentData
{
	string inputFilePath;
	unsigned int experimentNum;
	unsigned int duration;
	unsigned int repetitions;
	unsigned int x;
	unsigned int y;
	map<Type, Parameter> params;
	Results results;

	ExperimentData(const char *inputFile)
	{
		ifstream data;
		data.open(inputFile);
		if (!data.good())
		{
			throw runtime_error("Problem with experiment definition file! Check if the relative path is correct!");
		}

		inputFilePath = inputFile;
		string tmp;
		data >> tmp;
		experimentNum = stoi(tmp);
		data >> tmp;
		duration = stoi(tmp);
		data >> tmp;
		repetitions = stoi(tmp);
		data >> tmp;
		x = stoi(tmp);
		data >> tmp;
		y = stoi(tmp);

		int incCouter = 0;
		data >> tmp;
		params[Type::Cows].begin = stod(tmp);
		data >> tmp;
		params[Type::Cows].end = stod(tmp);
		data >> tmp;
		params[Type::Cows].inc = stod(tmp);
		if (params[Type::Cows].inc > 0)
			incCouter++;

		data >> tmp;
		params[Type::InfRate].begin = stod(tmp);
		data >> tmp;
		params[Type::InfRate].end = stod(tmp) + 0.000001;
		data >> tmp;
		params[Type::InfRate].inc = stod(tmp);
		if (params[Type::InfRate].inc > 0)
			incCouter++;

		data >> tmp;
		params[Type::RecRate].begin = stod(tmp);
		data >> tmp;
		params[Type::RecRate].end = stod(tmp) + 0.000001;
		data >> tmp;
		params[Type::RecRate].inc = stod(tmp);
		if (params[Type::RecRate].inc > 0)
			incCouter++;

		data >> tmp;
		params[Type::InfRange].begin = stod(tmp);
		data >> tmp;
		params[Type::InfRange].end = stod(tmp);
		data >> tmp;
		params[Type::InfRange].inc = stod(tmp);
		if (params[Type::InfRange].inc > 0)
			incCouter++;

		data.close();
		if (incCouter == 0 or incCouter > 2)
		{
			throw runtime_error("Experiment definition file is wrongly configured!\n Max two params can have non zero <increment>.");
		}
	}
};

size_t get_seed()
{
	random_device entropy;
	return entropy();
}

// algorithm based on Bresenham's line algorithm
void line(const Point &start, const Point &target, vector<Point> &way)
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

void initialize(vector<Cow> &cows, Generator &areaGen, double x, mt19937 &xEngine, double y, mt19937 &yEngine)
{
	for (unsigned i = 0; i < cows.size(); i++)
	{
		auto area = areaGen();
		if (area < 35000)
		{ // trawa
			Distribution xDist(0, (0.5 * x) - 1);
			Distribution yDist(0, 0.05 * y);
			cows[i].position.first = xDist(xEngine);
			cows[i].position.second = yDist(yEngine);
			cows[i].target.first = xDist(xEngine);
			cows[i].target.second = yDist(yEngine);
		}
		else if (area >= 35000 and area < 70000)
		{ // woda
			Distribution xDist(0.5 * x, x);
			Distribution yDist(0, 0.05 * y);
			cows[i].position.first = xDist(xEngine);
			cows[i].position.second = yDist(yEngine);
			cows[i].target.first = xDist(xEngine);
			cows[i].target.second = yDist(yEngine);
		}
		else
		{ // pozostały teren
			Distribution xDist(0, x);
			Distribution yDist((0.05 * y) + 1, y);
			cows[i].position.first = xDist(xEngine);
			cows[i].position.second = yDist(yEngine);
			cows[i].target.first = xDist(xEngine);
			cows[i].target.second = yDist(yEngine);
		}
		line(cows[i].position, cows[i].target, cows[i].way);
	}
}

#ifdef SHOULD_DRAW
void moveCows(vector<Cow> &cows, Generator &areaGen, double x, mt19937 &xEngine, double y, mt19937 &yEngine,
			  Generator &speedGen, Generator &speedInfectedGen,
			  Generator &stayGen, Generator &stayInfectedGen, string &command)
#else
void moveCows(vector<Cow> &cows, Generator &areaGen, double x, mt19937 &xEngine, double y, mt19937 &yEngine,
			  Generator &speedGen, Generator &speedInfectedGen,
			  Generator &stayGen, Generator &stayInfectedGen)
#endif
{
	for (unsigned i = 0; i < cows.size(); i++)
	{
		if (cows[i].state == State::Removed)
		{
			continue;
		}

		if (cows[i].target == cows[i].position)
		{
			auto area = areaGen();
			if (area < 40000)
			{ // trawa
				Distribution xDist(0, (0.5 * x) - 1);
				Distribution yDist(0, 0.05 * y);
				cows[i].target.first = xDist(xEngine);
				cows[i].target.second = yDist(yEngine);
			}
			else if (area >= 40000 and area < 80000)
			{ // woda
				Distribution xDist(0.5 * x, x);
				Distribution yDist(0, 0.05 * y);
				cows[i].target.first = xDist(xEngine);
				cows[i].target.second = yDist(yEngine);
			}
			else
			{ // pozostały teren
				Distribution xDist(0, x);
				Distribution yDist((0.05 * y) + 1, y);
				cows[i].target.first = xDist(xEngine);
				cows[i].target.second = yDist(yEngine);
			}
			cows[i].way.clear();
			cows[i].waypoint = 0;
			line(cows[i].position, cows[i].target, cows[i].way);
			if (cows[i].state == State::Infectious)
			{
				cows[i].stayTime = stayInfectedGen();
			}
			else
			{
				cows[i].stayTime = stayGen();
			}
		}

		if (cows[i].stayTime == 0)
		{
			if (cows[i].state == State::Infectious)
			{
				cows[i].waypoint += speedInfectedGen();
			}
			else
			{
				cows[i].waypoint += speedGen();
			}

			if (cows[i].waypoint + 1 >= cows[i].way.size())
			{
				cows[i].position.first = cows[i].target.first;
				cows[i].position.second = cows[i].target.second;
				cows[i].waypoint = cows[i].way.size() - 1;
			}
			else
			{
				cows[i].position.first = cows[i].way[cows[i].waypoint].first;
				cows[i].position.second = cows[i].way[cows[i].waypoint].second;
			}
		}
		else
		{
			cows[i].stayTime -= 1;
		}
#ifdef SHOULD_DRAW
		command = command + " '-' with point lc \"" + color.at(i) + "\" notitle,";
#endif
	}
}

double getDistance(Cow &cow1, Cow &cow2)
{
	return sqrt(pow((cow1.position.first - cow2.position.first), 2) + pow((cow1.position.second - cow2.position.second), 2));
}

bool spreadDisease(vector<Cow> &cows, mt19937 &probabilityEngine, Illness &illness, int experimentNum)
{
	bool areAnyInfectious = false;
	bernoulli_distribution recoverProbDist(illness.recoverRate);

	for (auto &cow1 : cows)
	{
		if (cow1.state == State::Infectious)
		{
			for (auto &cow2 : cows)
			{
				auto distance = getDistance(cow1, cow2);
				if ((cow2.state == State::Susceptible) and (distance <= illness.infectionRange))
				{
					cow2.isInRange = true;
					if ((cow2.exposition >= 10) or (experimentNum == 1))
					{
						// additional coefficient in E3 (1.0 - (distance / illness.infectionRange)) / 4.0
						double distanceCoefficient = 1.0 + (experimentNum >= 3 ? ((1.0 - (distance / illness.infectionRange)) / 4.0) : 0.0);

						auto prob = illness.infectionRate * distanceCoefficient;
						bernoulli_distribution probabilityDist(prob > 1.0 ? 1.0 : prob);
						if (probabilityDist(probabilityEngine))
						{
							if (experimentNum != 4)
							{
								cow2.state = State::Infectious;
								cow2.infectionTime = illness.infectionDuration;
							}
							else
							{
								cow2.state = State::Exposed;
								Distribution expositionDist(1800, 7200);
								cow2.incubationTime = expositionDist(probabilityEngine);
							}
						}
					}
				}
			}

			cow1.infectionTime -= 1;
			if (cow1.infectionTime == 0)
			{
				if (recoverProbDist(probabilityEngine))
				{
					cow1.state = State::Recovered;
				}
				else
				{
					cow1.state = State::Removed;
				}
			}
			else
			{
				areAnyInfectious = true;
			}
		}
		else if (cow1.state == State::Exposed)
		{
			if (cow1.incubationTime == 0)
			{
				cow1.state = State::Infectious;
				cow1.infectionTime = illness.infectionDuration;
			}
			else
			{
				cow1.incubationTime--;
			}
		}
	}

	if (experimentNum > 1)
	{
		for (auto &cow : cows)
		{
			if (cow.isInRange)
			{
				cow.exposition++;
				cow.isInRange = false;
			}
			else
			{
				cow.exposition = 0;
			}
		}
	}

	return areAnyInfectious;
}

void prepareOutput(ExperimentData &def, SingleResults &results)
{
	vector<Type> types;
	for (auto &param : def.params)
	{
		if (param.second.inc != 0.0)
		{
			types.push_back(param.first);
		}
	}
	auto cowsNum = def.params[Type::Cows].current;

	if (types.size() == 1) // single line - one data serie
	{
		def.results[State::Infectious][def.params[types[0]].current].push_back(results[State::Infectious] / cowsNum);
		def.results[State::Recovered][def.params[types[0]].current].push_back(results[State::Recovered] / cowsNum);
		def.results[State::Removed][def.params[types[0]].current].push_back(results[State::Removed] / cowsNum);
	}
	else
	{
		auto set0 = def.params[types[0]];
		auto set1 = def.params[types[1]];
		auto diff0 = set0.end - set0.begin;
		auto diff1 = set1.end - set1.begin;
		if ((set0.inc / diff0) < (set1.inc / diff1))
		{
			def.results[State::Infectious][set0.current].push_back(results[State::Infectious] / cowsNum);
			def.results[State::Recovered][set0.current].push_back(results[State::Recovered] / cowsNum);
			def.results[State::Removed][set0.current].push_back(results[State::Removed] / cowsNum);
		}
		else
		{
			def.results[State::Infectious][set1.current].push_back(results[State::Infectious] / cowsNum);
			def.results[State::Recovered][set1.current].push_back(results[State::Recovered] / cowsNum);
			def.results[State::Removed][set1.current].push_back(results[State::Removed] / cowsNum);
		}
	}
}

void printResults(ExperimentData &def)
{
	auto fileName = def.inputFilePath.substr(def.inputFilePath.rfind("/") + 1);
	string outputInfectiousName = "output_Infectious_" + fileName;
	string outputRecoveredName = "output_Recovered_" + fileName;
	string outputRemovedName = "output_Removed_" + fileName;

	map<State, ofstream> files;
	files[State::Infectious].open(outputInfectiousName.c_str(), ofstream::out);
	files[State::Recovered].open(outputRecoveredName.c_str(), ofstream::out);
	files[State::Removed].open(outputRemovedName.c_str(), ofstream::out);
	if ((!files[State::Infectious].good()) or (!files[State::Recovered].good()) or (!files[State::Removed].good()))
	{
		throw runtime_error("Problem with output files!");
	}

	for (auto &set : def.results)
	{
		for (auto &res : set.second)
		{
			files[set.first] << res.first;
			for (auto &val : res.second)
			{
				files[set.first] << "\t" << val / def.repetitions;
			}
			files[set.first] << endl;
		}
		files[set.first].close();
	}
}

void experiment(const char *inputFile)
{
	ExperimentData def(inputFile);

	for (def.params[Type::Cows].init(); def.params[Type::Cows].check(); def.params[Type::Cows].incr())
	{
		for (def.params[Type::InfRate].init(); def.params[Type::InfRate].check(); def.params[Type::InfRate].incr())
		{
			for (def.params[Type::RecRate].init(); def.params[Type::RecRate].check(); def.params[Type::RecRate].incr())
			{
				for (def.params[Type::InfRange].init(); def.params[Type::InfRange].check(); def.params[Type::InfRange].incr())
				{
					Illness illness(def.params[Type::InfRate].current, def.params[Type::RecRate].current, def.params[Type::InfRange].current);

					mt19937 areaEngine(get_seed());
					Distribution areaDist(0, 100000);
					auto areaGen = bind(areaDist, areaEngine);

					mt19937 xEngine(get_seed());
					mt19937 yEngine(get_seed());

					mt19937 speedEngine(get_seed());
					Distribution speedDist(11, 23);
					auto speedGen = bind(speedDist, speedEngine);
					Distribution speedInfectedDist(5, 11);
					auto speedInfectedGen = bind(speedInfectedDist, speedEngine);

					mt19937 stayEngine(get_seed());
					Distribution stayDist(0, 1800);
					auto stayGen = bind(stayDist, stayEngine);
					Distribution stayInfectedDist(60, 3600);
					auto stayInfectedGen = bind(stayInfectedDist, stayEngine);

					mt19937 probabilityEngine(get_seed());

#ifdef SHOULD_DRAW
					Gnuplot gp;
					gp << "set xrange [0:" + to_string(def.x) + "]\n";
					gp << "set yrange [0:" + to_string(def.y) + "]\n";
#endif

					vector<Cow> cows(def.params[Type::Cows].current);
					SingleResults results = {
						{State::Exposed, 0.0},
						{State::Infectious, 0.0},
						{State::Recovered, 0.0},
						{State::Removed, 0.0}};
					unsigned int durationInSec = def.duration * 60 * 60;
					for (size_t i = 0; i < def.repetitions; i++)
					{
						initialize(cows, areaGen, def.x, xEngine, def.y, yEngine);
						cows[0].state = State::Infectious;
						cows[0].infectionTime = illness.infectionDuration;
						Clock clock;
						for (unsigned int j = 0; j < durationInSec; j++)
						{
#ifdef SHOULD_DRAW
							string command = "plot";
#endif

							if (!clock.isSleepTime())
							{
#ifdef SHOULD_DRAW
								moveCows(cows, areaGen, def.x, xEngine, def.y, yEngine, speedGen, speedInfectedGen, stayGen, stayInfectedGen, command);
#else
								moveCows(cows, areaGen, def.x, xEngine, def.y, yEngine, speedGen, speedInfectedGen, stayGen, stayInfectedGen);
#endif
							}

							if (!spreadDisease(cows, probabilityEngine, illness, def.experimentNum))
							{
								int a = 0;
								for (auto &cow : cows)
								{
									if (cow.state == State::Susceptible)
										a++;
								}
								//cout << "Epidemy stopped at " << clock.to_string() << " with " << a << "suvivors and P = " << def.params[Type::InfRate].current << endl;
								break;
							}

#ifdef SHOULD_DRAW
							gp << "set title \"" + clock.to_string() + "\"\n";
							gp << command + "\n";
							for (size_t i = 0; i < cows.size(); i++)
							{
								vector<pair<double, double>> pts;
								pts.push_back(make_pair(double(cows[i].position.first), double(cows[i].position.second)));
								gp.send1d(pts);
							}
							gp.flush();
							mysleep(2);
#endif
							clock.tick();
						}

						for (auto &ent : cows)
						{
							if (ent.state != State::Susceptible)
							{
								results[ent.state]++;
							}
							ent.clear();
						}
					}

					prepareOutput(def, results);

#ifdef SHOULD_DRAW
					gp << "reset";
#endif
				}
			}
		}
	}

	printResults(def);
}

int main(int argc, char **argv)
{
	try
	{
		int c;
		char *inputFile;
		while ((c = getopt(argc, argv, "f:")) != -1)
			switch (c)
			{
			case 'f':
				inputFile = optarg;
				experiment(inputFile);
				break;
			default:
				break;
			}
	}
	catch (const exception &e)
	{
		cerr << e.what() << '\n';
		return 1;
	}

	return 0;
}