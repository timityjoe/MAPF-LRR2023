#include "CompetitionSystem.h"
#include "Evaluation.h"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>
#include "nlohmann/json.hpp"
#include <signal.h>
#include <climits>
#include <memory>


#ifdef PYTHON
#if PYTHON
#include "pyMAPFPlanner.hpp"
#include <pybind11/embed.h>
#endif
#endif

namespace po = boost::program_options;
using json = nlohmann::json;

po::variables_map vm;
std::unique_ptr<BaseSystem> system_ptr;


void sigint_handler(int a)
{
    fprintf(stdout, "stop the simulation...\n");
    if (!vm["evaluationMode"].as<bool>())
    {
        system_ptr->saveResults(vm["output"].as<std::string>(),vm["outputScreen"].as<int>());
    }
    _exit(0);
}


int main(int argc, char **argv)
{
#ifdef PYTHON
    // st::cout<<"Using Python="<<PYTHON<<std::endl;
#if PYTHON
    pybind11::initialize_interpreter();
#endif
#endif

    printf("(0) Main() - Start \n");

    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")
        // ("inputFolder", po::value<std::string>()->default_value("."), "input folder")
        ("inputFile,i", po::value<std::string>()->required(), "input file name")
        ("output,o", po::value<std::string>()->default_value("./test.json"), "output file name")
        ("outputScreen", po::value<int>()->default_value(1), "the level of details in the output file, 1--showing all the output, 2--ignore the events and tasks, 3--ignore the events, tasks, errors, planner times, starts and paths")
        ("evaluationMode", po::value<bool>()->default_value(false), "evaluate an existing output file")
        ("simulationTime", po::value<int>()->default_value(5000), "run simulation")
        ("fileStoragePath", po::value<std::string>()->default_value(""), "the path to the storage path")
        ("planTimeLimit", po::value<int>()->default_value(INT_MAX), "the time limit for planner in seconds")
        ("preprocessTimeLimit", po::value<int>()->default_value(INT_MAX), "the time limit for preprocessing in seconds")
        ("logFile,l", po::value<std::string>()->default_value(""), "issue log file name");
    clock_t start_time = clock();
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 1;
    }

    po::notify(vm);

    // std::string base_folder = vm["inputFolder"].as<std::string>();
    boost::filesystem::path p(vm["inputFile"].as<std::string>());
    boost::filesystem::path dir = p.parent_path();
    std::string base_folder = dir.string();
    if (base_folder.size() > 0 && base_folder.back() != '/')
    {
        base_folder += "/";
    }

    Logger *logger = new Logger(vm["logFile"].as<std::string>());

    printf("(1) Main() - Init Planner \n");

    MAPFPlanner *planner = nullptr;
    // Planner is inited here, but will be managed and deleted by system_ptr deconstructor
    if (vm["evaluationMode"].as<bool>())
    {
        logger->log_info("running the evaluation mode");
        planner = new DummyPlanner(vm["output"].as<std::string>());
    }
    else
    {
#ifdef PYTHON
#if PYTHON
        planner = new pyMAPFPlanner();
#else
        planner = new MAPFPlanner();
#endif
#endif
    }

    auto input_json_file = vm["inputFile"].as<std::string>();
    json data;
    std::ifstream f(input_json_file);
    try
    {
        data = json::parse(f);
    }
    catch (json::parse_error error)
    {
        std::cerr << "Failed to load " << input_json_file << std::endl;
        std::cerr << "Message: " << error.what() << std::endl;
        exit(1);
    }

    auto map_path = read_param_json<std::string>(data, "mapFile");
    Grid grid(base_folder + map_path);

    planner->env->map_name = map_path.substr(map_path.find_last_of("/") + 1);
    planner->env->file_storage_path = vm["fileStoragePath"].as<std::string>();

    ActionModelWithRotate *model = new ActionModelWithRotate(grid);
    model->set_logger(logger);

    int team_size = read_param_json<int>(data, "teamSize");

    std::vector<int> agents = read_int_vec(base_folder + read_param_json<std::string>(data, "agentFile"), team_size);
    std::vector<int> tasks = read_int_vec(base_folder + read_param_json<std::string>(data, "taskFile"));

    printf("(2) Main() - tasks.size():%i \n", tasks.size());

    // *Print out a few values
    std::vector<int>::iterator iter = tasks.begin();
    for(int i = 0; i < 10; i++)
    {
        int val = *iter;
        printf("    i:%i, val:%i \n", i, val);
        iter++;
    }

    printf("(3) Main() agents.size():%i \n", agents.size());

    if (agents.size() > tasks.size())
        logger->log_warning("Not enough tasks for robots (number of tasks < team size)");

    std::string task_assignment_strategy = data["taskAssignmentStrategy"].get<std::string>();

    printf("task_assignment_strategy:%s \n", task_assignment_strategy.c_str());

    if (task_assignment_strategy == "greedy")
    {
        system_ptr = std::make_unique<TaskAssignSystem>(grid, planner, agents, tasks, model);
    }
    else if (task_assignment_strategy == "roundrobin")
    {
        system_ptr = std::make_unique<InfAssignSystem>(grid, planner, agents, tasks, model);
    }
    else if (task_assignment_strategy == "roundrobin_fixed")
    {
        std::vector<vector<int>> assigned_tasks(agents.size());
        for (int i = 0; i < tasks.size(); i++)
        {
            assigned_tasks[i % agents.size()].push_back(tasks[i]);
        }
        system_ptr = std::make_unique<FixedAssignSystem>(grid, planner, agents, assigned_tasks, model);
    }
    else
    {
        std::cerr << "unkown task assignment strategy " << data["taskAssignmentStrategy"].get<std::string>() << std::endl;
        logger->log_fatal("unkown task assignment strategy " + data["taskAssignmentStrategy"].get<std::string>());
        exit(1);
    }

    system_ptr->set_logger(logger);
    system_ptr->set_plan_time_limit(vm["planTimeLimit"].as<int>());
    system_ptr->set_preprocess_time_limit(vm["preprocessTimeLimit"].as<int>());

    system_ptr->set_num_tasks_reveal(read_param_json<int>(data, "numTasksReveal", 1));

    signal(SIGINT, sigint_handler);

    printf("(4) Main() - Running Simulation... \n\n");
    system_ptr->simulate(vm["simulationTime"].as<int>());

    if (!vm["evaluationMode"].as<bool>())
    {
        system_ptr->saveResults(vm["output"].as<std::string>(),vm["outputScreen"].as<int>());
    }

    printf("(5) Main() - End! \n\n");

    delete model;
    delete logger;
    _exit(0);
}
