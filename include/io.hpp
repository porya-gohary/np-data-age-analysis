#ifndef IO_HPP
#define IO_HPP

#include <iostream>
#include <utility>

#include "interval.hpp"
#include "time.hpp"
#include "jobs.hpp"
#include "task.hpp"
#include "dag.hpp"
#include "precedence.hpp"
#include "aborts.hpp"
#include "yaml-cpp/yaml.h"

namespace NP {

	template<class Time>
	typename NP::dag<Time> parse_mr_dag(std::istream &in, bool want_worst_case = false) {
		// make a dag
		NP::dag<Time> dag;
		unsigned long tid;
		Time bcet, wcet, period, deadline, jitter;
		Time rec_cost_min = 0, rec_cost_max = 0;
		try {
			YAML::Node input_vertex_set = YAML::Load(in);

			auto const ts = input_vertex_set["vertexset"];
			for (auto const &t: ts) {
				tid = t["TaskID"].as<unsigned long>();
				bcet = want_worst_case ? t["WCET"].as<Time>() : t["BCET"].as<Time>();
				wcet = t["WCET"].as<Time>();
				if (t["RecoveryCostMin"]) {
					rec_cost_min = want_worst_case ? t["RecoveryCostMax"].as<Time>() : t["RecoveryCostMin"].as<Time>();
				} else{
					rec_cost_min = want_worst_case ? t["WCET"].as<Time>() : t["BCET"].as<Time>();
				}
				if (t["RecoveryCostMax"]) {
					rec_cost_max = t["RecoveryCostMax"].as<Time>();
				} else {
					rec_cost_max = t["WCET"].as<Time>();
				}
				period = t["Period"].as<Time>();
				deadline = t["Deadline"].as<Time>();

				jitter = want_worst_case ? 0:t["Jitter"].as<Time>();
				unsigned long PE = t["PE"].as<unsigned long>();
				dag.add_task(tid, bcet, wcet, rec_cost_min, rec_cost_max, period, Interval<Time>{0, jitter}, deadline, PE);
			}

			// now add the edges

			for (auto const &t: ts) {
				if (t["Successors"]) {
					// find the task with the given tid
					auto curr_tid = t["TaskID"].as<unsigned long>();
					auto const succs = t["Successors"];
					for (auto const &s: succs) {
						auto succ_tid = s.as<unsigned long>();

						//make an edge
						dag.add_edge(curr_tid, succ_tid);
					}
				}
			}


		} catch (const YAML::Exception &e) {
			std::cerr << "Error reading YAML file: " << e.what() << std::endl;
		}
		return dag;
	}

	template<class Time>
	void parse_task_chain(std::istream &in, typename NP::dag<Time> &dag) {
		// Clear any flags
		in.clear();
		// Move the pointer to the beginning
		in.seekg(0, std::ios::beg);

		try {
			YAML::Node input_chains = YAML::Load(in);

			auto const ch = input_chains["taskchains"];
			for (auto const &t: ch) {
				auto const chain = t["Chain"];
				typename NP::dag<Time>::Task_chain temp_chain;
				for (auto const &c: chain) {
					// find the task with the given tid
					auto tid = c.as<unsigned long>();
					auto task = dag.find_task(tid);
					temp_chain.push_back(task);
				}
				dag.add_task_chain(temp_chain);
			}

		} catch (const YAML::Exception &e) {
			std::cerr << "Error reading YAML file: " << e.what() << std::endl;
		}
	}


	// function to generate job set out of a dag with given hyperperiod
	template<class Time> typename Job<Time>::Job_set generate_job_set(typename NP::dag<Time> dag) {
		typename Job<Time>::Job_set jobs;
		auto tasks = dag.get_tasks();
		dag.calculate_hyperperiod();
		auto hyperperiod = dag.get_hyperperiod();
		auto observation_window = dag.get_number_hp_observation_window() * hyperperiod;
		long long id_counter = 0;
		for (auto &t_instance: tasks) {
			for (long long i = 0; i < observation_window; i += t_instance->get_period()) {
				auto tid = t_instance->get_task_id();
				auto jid = id_counter;
				auto arr_min = i + t_instance->min_jitter();
				auto arr_max = i + t_instance->max_jitter();
				auto cost_min = t_instance->get_bcet();
				auto cost_max = t_instance->get_wcet();
				auto rec_cost = t_instance->get_rec_cost();
				auto dl = i + t_instance->get_deadline();
				auto pe = t_instance->get_pe();

				// for EDF priority is deadline
				auto prio = dl;
//				auto prio = t_instance->get_task_id();
				// for now, let's consider the recovery priority to be the same as the task priority
				auto rec_prio = prio;
				jobs.emplace_back(jid, Interval<Time>{arr_min, arr_max}, Interval<Time>{cost_min, cost_max},
								  rec_cost, dl, prio, rec_prio, tid, pe);

				id_counter++;
			}
		}
		return jobs;
	}


}

#endif
