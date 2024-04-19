#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#ifndef _WIN32

#include <sys/resource.h>

#endif

#include "OptionParser.h"
#include "csvfile.hpp"
#include "config.h"


#include "problem.hpp"
#include "uni/space.hpp"
#include "uni/por_space.hpp"
#include "uni/por_criterion.hpp"
#include "uni/reduction_set.hpp"
#include "data_age_analysis.hpp"
#include "io.hpp"
#include "clock.hpp"


#define MAX_PROCESSORS 512

// command line options
static bool want_naive;
static bool want_dense;
static bool want_worst_case;

static bool want_precedence = false;
static std::string precedence_file;

static bool want_multiprocessor = false;
static unsigned int num_processors = 1;


#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
static bool want_dot_graph;
#endif
static double timeout;
static unsigned int max_depth = 0;

static bool want_rta_file;

static bool continue_after_dl_miss = false;



template<class Time>
struct Analysis_result {
	bool schedulable = true;
	bool timeout = false;
	unsigned long long number_of_states = 0, number_of_edges = 0, max_width = 0, number_of_jobs = 0;
	double cpu_time = 0;
	std::unordered_map<NP::JobID, Interval<Time>> rta;
	std::unordered_map<NP::JobID, Interval<Time>> sta;
	std::string graph;
	std::string response_times_csv;
};

template<class Time, class Space>
Analysis_result<Time> analyze(
		std::istream &in, const std::string &fname) {

	// first read the task set file
	auto dag = NP::parse_mr_dag<Time>(in, want_worst_case);

	// read task chains from file
	NP::parse_task_chain(in, dag);

	//if task chains are not in the input file, use default chains
	if (dag.get_task_chains().empty()) {
//		dag.find_task_chains();
		dag.find_longest_task_chain();
	}


	// generate a job set from dag
	auto jobs = NP::generate_job_set<Time>(dag);



	// now we have to run the main analysis
	Analysis_result<Time> main_result;
	auto graph = std::ostringstream();
	// we have to separate the jobs for each processor
	for (int i = 0; i < num_processors; ++i) {
		typename NP::Job<Time>::Job_set par_jobs;
		std::vector<std::size_t> indexes;
		for (int j = 0; j < jobs.size(); ++j) {
			if (jobs[j].get_pe() == i) {
				par_jobs.push_back(jobs[j]);
				indexes.push_back(j);
			}
		}
		if (par_jobs.empty()) {
			continue;
		}


		NP::Scheduling_problem<Time> main_problem{par_jobs,1};

		// Set common analysis options
		NP::Analysis_options opts;
		opts.timeout = timeout;
		opts.max_depth = max_depth;
		opts.early_exit = !continue_after_dl_miss;
		opts.num_buckets = main_problem.jobs.size();
		opts.be_naive = want_naive;

		// Actually call the analysis engine
		auto space = Space::explore(main_problem, opts);

		// Extract the analysis results
		main_result.schedulable = main_result.schedulable && space.is_schedulable();
		main_result.timeout = main_result.timeout || space.was_timed_out();
		main_result.number_of_states += space.number_of_states();
		main_result.number_of_edges += space.number_of_edges();
		main_result.max_width = std::max(main_result.max_width,
										 (unsigned long long) space.max_exploration_front_width());
		main_result.number_of_jobs += par_jobs.size();
		main_result.cpu_time += space.get_cpu_time();

		for (int j = 0; j < par_jobs.size(); ++j) {
			Interval<Time> start = space.get_start_times(main_problem.jobs[j]);
			Interval<Time> finish = space.get_finish_times(main_problem.jobs[j]);

			// add the start and finish times to the main result
			main_result.sta.emplace(par_jobs[j].get_id(), start);
			main_result.rta.emplace(par_jobs[j].get_id(), finish);

		}
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
		if (want_dot_graph)
		graph << space;
#endif
	}

	//
	main_result.graph = graph.str();

	auto rta = std::ostringstream();

	if (want_rta_file) {
		rta << "Task ID, Job ID, BCCT, WCCT, BCRT, WCRT" << std::endl;
		for (const auto &j: jobs) {
			Interval<Time> finish = main_result.rta.find(j.get_id())->second;
			rta << j.get_task_id() << ", "
				<< j.get_job_id() << ", "
				<< finish.from() << ", "
				<< finish.until() << ", "
				<< std::max<long long>(0,
									   (finish.from() - j.earliest_arrival()))
				<< ", "
				<< (finish.until() - j.earliest_arrival())
				<< std::endl;
		}
	}
	main_result.response_times_csv = rta.str();
	if (main_result.schedulable) {
		int index = 0;
		csvfile csv_DA("results_DA.csv", true, ",");
		// now we perform the latency analysis
		for (const auto &tc: dag.get_task_chains()) {
			// skip if the chain has only one task
			if (tc.size() == 1) {
				index++;
				continue;
			}
			std::string chain_string;
			for (int j = 0; j < tc.size(); ++j) {
				if (j != 0) {
					chain_string.append(" -> " + tc[j]->get_name());
				} else {
					chain_string.append(tc[j]->get_name());
				}
			}
			Time HPchain = dag.get_chain_hyperperiod(index);
			auto latency_analysis = NP::Data_age_analysis<Time>(jobs, main_result.sta, main_result.rta, tc);
			auto data_age_bound = latency_analysis.get_data_age();

			DM ("Chain: " << chain_string << std::endl);
			DM ("Data Age Bound: " << (double) data_age_bound.from() << " - "
					  << (double) data_age_bound.until() << std::endl);

			auto label = fname;
			label.append(" - ").append(std::to_string(index));
			csv_DA << label;
			csv_DA << (double) data_age_bound.from();
			csv_DA << (double) data_age_bound.until();
			csv_DA << "";
			csv_DA << endrow;
			index++;
		}
	}

	return main_result;
}

template<class Time>
static Analysis_result<Time> process_stream(
		std::istream &in, const std::string &fname) {
	return analyze<Time,NP::Uniproc::Por_state_space<Time, NP::Uniproc::Null_IIP<Time>, NP::Uniproc::POR_release_order<Time>>>(in, fname);

}

template<class Time>
static void process_file(const std::string &fname) {
	try {
		Analysis_result<Time> result;

		auto empty_dag_stream = std::istringstream("\n");
		auto empty_aborts_stream = std::istringstream("\n");
		auto dag_stream = std::ifstream();
		auto aborts_stream = std::ifstream();


		if (fname == "-")
			result = process_stream<Time>(std::cin, "-");
		else {
			auto in = std::ifstream(fname, std::ios::in);
			result = process_stream<Time>(in, fname);
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
			if (want_dot_graph) {
				std::string dot_name = fname;
				auto p = dot_name.find(".yaml");
				// add the main graph scope to the graph
				result.graph.insert(0, "digraph G {\n");
				result.graph.append("}");
				if (p != std::string::npos) {
					dot_name.replace(p, std::string::npos, ".dot");
					auto out  = std::ofstream(dot_name,  std::ios::out);
					out << result.graph;
					out.close();
				}
			}
#endif
			if (want_rta_file) {
				std::string rta_name = fname;
				auto p = rta_name.find(".yaml");
				if (p != std::string::npos) {
					rta_name.replace(p, std::string::npos, ".rta.csv");
					auto out = std::ofstream(rta_name, std::ios::out);
					out << result.response_times_csv;
					out.close();
				}
			}
		}

#ifdef _WIN32 // rusage does not work under Windows
		long mem_used = 0;
#else
		struct rusage u;
		long mem_used = 0;
		if (getrusage(RUSAGE_SELF, &u) == 0)
			mem_used = u.ru_maxrss;
#endif

		std::cout << fname;

		if (max_depth && max_depth < result.number_of_jobs)
			// mark result as invalid due to debug abort
			std::cout << ",  X";
		else
			std::cout << ",  " << (int) result.schedulable;

		std::cout << ",  " << result.number_of_jobs
				  << ",  " << result.number_of_states
				  << ",  " << result.number_of_edges
				  << ",  " << result.max_width
				  << ",  " << std::fixed << result.cpu_time
				  << ",  " << ((double) mem_used) / (1024.0)
				  << ",  " << (int) result.timeout
				  << ",  " << num_processors
				  << std::endl;
	} catch (std::ios_base::failure &ex) {
		std::cerr << fname;
		if (want_precedence)
			std::cerr << " + " << precedence_file;
		std::cerr << ": parse error" << std::endl;
		exit(1);
	} catch (NP::InvalidJobReference &ex) {
		std::cerr << precedence_file << ": bad job reference: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " is not part of the job set given in "
				  << fname
				  << std::endl;
		exit(3);
	} catch (std::exception &ex) {
		std::cerr << fname << ": '" << ex.what() << "'" << std::endl;
		exit(1);
	}
}

static void print_header() {
	std::cout << "# file name"
			  << ", schedulable?"
			  << ", #jobs"
			  << ", #states"
			  << ", #edges"
			  << ", max width"
			  << ", CPU time"
			  << ", memory"
			  << ", timeout"
			  << ", #CPUs"
			  << ", #recovery blocks"
			  << std::endl;
}

int main(int argc, char **argv) {
	auto parser = optparse::OptionParser();

	parser.description("Data-age Analysis for Multi-rate Task chains");
	parser.usage("usage: %prog [OPTIONS]... [DAG Task]...");

	parser.add_option("-m", "--multiprocessor").dest("num_processors")
			.help("set the number of processors of the platform")
			.set_default("1");

	parser.add_option("-t", "--time").dest("time_model")
			.metavar("TIME-MODEL")
			.choices({"dense", "discrete"}).set_default("discrete")
			.help("choose 'discrete' or 'dense' time (default: discrete)");

	parser.add_option("-l", "--time-limit").dest("timeout")
			.help("maximum CPU time allowed (in seconds, zero means no limit)")
			.set_default("0");

	parser.add_option("-d", "--depth-limit").dest("depth")
			.help("abort graph exploration after reaching given depth (>= 2)")
			.set_default("0");

	parser.add_option("-n", "--naive").dest("naive").set_default("0")
			.action("store_const").set_const("1")
			.help("use the naive exploration method (default: merging)");

	parser.add_option("-w", "--wcet").dest("worse_case").set_default("0")
			.action("store_const").set_const("1")
			.help("use WCET as actual execution time and zero jitter for every job (default: off)");

	parser.add_option("--header").dest("print_header")
			.help("print a column header")
			.action("store_const").set_const("1")
			.set_default("0");

	parser.add_option("-g", "--save-graph").dest("dot").set_default("0")
			.action("store_const").set_const("1")
			.help("store the state graph in Graphviz dot format (default: off)");

	parser.add_option("-r", "--save-response-times").dest("rta").set_default("0")
			.action("store_const").set_const("1")
			.help("store the best- and worst-case response times (default: off)");

	parser.add_option("-c", "--continue-after-deadline-miss")
			.dest("go_on_after_dl").set_default("0")
			.action("store_const").set_const("1")
			.help("do not abort the analysis on the first deadline miss "
				  "(default: off)");


	auto options = parser.parse_args(argc, argv);

	const std::string &time_model = options.get("time_model");
	want_dense = time_model == "dense";

	want_naive = options.get("naive");

	timeout = options.get("timeout");

	max_depth = options.get("depth");
	if (options.is_set_by_user("depth")) {
		if (max_depth <= 1) {
			std::cerr << "Error: invalid depth argument\n" << std::endl;
			return 1;
		}
		max_depth -= 1;
	}


	want_multiprocessor = options.is_set_by_user("num_processors");
	num_processors = options.get("num_processors");
	if (!num_processors || num_processors > MAX_PROCESSORS) {
		std::cerr << "Error: invalid number of processors\n" << std::endl;
		return 1;
	}


	want_rta_file = options.get("rta");

	want_worst_case = options.get("worse_case");

	continue_after_dl_miss = options.get("go_on_after_dl");

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	want_dot_graph = options.get("dot");
#else
	if (options.is_set_by_user("dot")) {
		std::cerr << "Error: graph collection support must be enabled "
				  << "during compilation (CONFIG_COLLECT_SCHEDULE_GRAPH "
				  << "is not set)." << std::endl;
		return 2;
	}
#endif


	if (options.get("print_header"))
		print_header();

	for (auto f: parser.args())
		if (want_dense)
			process_file<dense_t>(f);
		else if (!want_dense)
			process_file<dtime_t>(f);

	if (parser.args().empty())
		if (want_dense)
			process_file<dense_t>("-");
		else if (!want_dense)
			process_file<dtime_t>("-");

	return 0;
}
