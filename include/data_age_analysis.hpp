
#ifndef DATA_AGE_ANALYSIS_HPP
#define DATA_AGE_ANALYSIS_HPP


#include "jobs.hpp"
#include "task.hpp"
#include "unordered_map"
#include "chrono"
namespace NP {
	template<class Time>
	class Data_age_analysis {
		typedef typename Job<Time>::Job_set Workload;
		typedef std::vector<std::shared_ptr<Task<Time>>> Task_chain;
		typedef std::unordered_map<NP::JobID, Interval<Time>> Response_times;


	private:
		Response_times rta;
		Response_times sta;
		const Workload &jobs;
		Task_chain chain;
		long elapsed_time;
		Interval<Time> data_age = {0, 0};
		bool pruning;
		bool preemptive = false;

	public:
		Data_age_analysis(const Workload &jobs, Response_times &sta, Response_times &rta, Task_chain taskChain,
						bool preemptive = false, bool pruning = false) : jobs(jobs),
												sta(sta),
												rta(rta),
												chain(taskChain),
												pruning(pruning),
												preemptive(preemptive) {
			auto tstart = std::chrono::high_resolution_clock::now();
			calculate_latencies(chain);
			auto tend = std::chrono::high_resolution_clock::now();
			elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(tend - tstart).count();
		}


		void calculate_latencies(Task_chain chain) {
			for (int i = 0; i < jobs.size(); i++) {
				if (jobs[i].get_task_id() == chain.back()->get_task_id()) {
					if (chain.size() == 1) {
						update_latencies(jobs[i], Interval<Time>{0, 0}, std::vector<Time>{}, chain);
					} else {
						std::vector<Time> origin = find_origin_jobs(std::vector<int>{i}, sta.find(jobs[i].get_id())->second.until(),
																	chain);
						if (!origin.empty()) update_latencies(jobs[i], rta.find(jobs[i].get_id())->second, origin, chain);
					}
				}

			}
		}

//    Recursive Algorithm to calculate possible origin jobs for a termination job
		std::vector<Time>
		find_origin_jobs(std::vector<int> origin_jobs, Time lst, Task_chain local_chain) {
//        this vector keeps the potential data-producer jobs by their earliest release time
			std::vector<Time> origin;
//        keep index of potential data-producer jobs
			std::vector<int> local_origin_jobs;
			if (local_chain.size() == 2) {
				for (auto &oj: origin_jobs) {

					auto data_producer = get_data_producer(jobs[oj].get_task_id(), local_chain);
					Interval<Time> start_time = sta.find(jobs[oj].get_id())->second;
					int first_index = get_first_index_of_data_producer(start_time, jobs[oj], data_producer);
					int last_index = get_last_index_of_data_producer(start_time, jobs[oj], data_producer, first_index);
					if (first_index != -1) {
						local_origin_jobs.push_back(first_index);
						origin.push_back(jobs[first_index].earliest_arrival());
					}
					first_index++;

					for (int i = first_index; i <= last_index; i++) {
						if (jobs[i].get_task_id() == data_producer->get_task_id()) {
//                              local origin jobs are just used for DEBUG purpose here
							if (sta.find(jobs[i].get_id())->second.from() < lst) {
//                            check the sanity condition for each potential data-producer job
								local_origin_jobs.push_back(i);
								origin.push_back(jobs[i].earliest_arrival());
							}
						}
					}


				}

				if (origin.size() > 2 && pruning)
					origin.erase(origin.begin() + 1, origin.end() - 1);
				return origin;

			} else {
				for (auto &oj: origin_jobs) {

					auto data_producer = get_data_producer(jobs[oj].get_task_id(), local_chain);
					Interval<Time> start_time = sta.find(jobs[oj].get_id())->second;
					int first_index = get_first_index_of_data_producer(start_time, jobs[oj], data_producer);
					int last_index = get_last_index_of_data_producer(start_time, jobs[oj], data_producer, first_index);

					if (first_index != -1) {
//                    If startIndex == -1 -> it shows that it is possible that job doesn't make a chain instance.
						local_origin_jobs.push_back(first_index);
					}
					first_index++;

					for (int i = first_index; i <= last_index; i++) {
						if (jobs[i].get_task_id() == data_producer->get_task_id()) {
							if (sta.find(jobs[i].get_id())->second.from() < lst) {
								local_origin_jobs.push_back(i);
							}
						}
					}


				}
				if (local_origin_jobs.size() > 2 && pruning)
					local_origin_jobs.erase(local_origin_jobs.begin() + 1, local_origin_jobs.end() - 1);
				local_chain.pop_back();
				return find_origin_jobs(local_origin_jobs, lst, local_chain);
			}
		}


		void update_latencies(const Job<Time> &j, Interval<Time> finish_time, std::vector<Time> origin,
							  Task_chain local_chain) {

			if (local_chain.size() == 1) {
				if (finish_time.until() == 0) {
					Interval<Time> latency = rta.find(j.get_id())->second;
					latency -= j.earliest_arrival();
					update_data_age(latency);
				}
			} else {
				for (int i = 0; i < origin.size(); i++) {
					Interval<Time> latency = finish_time;
					latency -= origin[i];
					update_data_age(latency);
				}
			}
		}


		int get_first_index_of_data_producer(Interval<Time> start_time, const Job<Time> &j,
											 std::shared_ptr<Task<Time>> data_producer) {
			if (j.get_pe() == data_producer->get_pe() && !preemptive) {
				for (int i = jobs.size() - 1; i >= 0; i--) {
					if (jobs[i].get_task_id() == data_producer->get_task_id()) {
						if (sta.find(jobs[i].get_id())->second.until() <= start_time.from()) {
							return i;
						}
					}
				}
			} else {
				for (int i = jobs.size() - 1; i >= 0; i--) {
					if (jobs[i].get_task_id() == data_producer->get_task_id()) {
						if (rta.find(jobs[i].get_id())->second.until() <= start_time.from()) {
							return i;
						}
					}
				}
			}
			return -1;
		}

		int get_last_index_of_data_producer(Interval<Time> start_time, const Job<Time> &j,
											std::shared_ptr<Task<Time>> data_producer, int first_index) {
			int temp = -1;
			for (int i = first_index + 1; i < jobs.size(); ++i) {
				if (jobs[i].get_task_id() == data_producer->get_task_id()) {
					if (rta.find(jobs[i].get_id())->second.from() <= start_time.until()) {
						temp = i;
					} else if (rta.find(jobs[i].get_id())->second.from() > start_time.until()) {
						return temp;
					}
				}
			}
			return -1;
		}

		//    a function to get data producer of a task
		std::shared_ptr<Task<Time>> get_data_producer(const unsigned long task_id, Task_chain chain) {
			for (int i = 0; i < chain.size(); i++) {
				if (chain[i]->get_task_id() == task_id && i != 0) {
					return chain[i - 1];
				}
			}
			assert(0);
			return nullptr;
		}

		void update_data_age(Interval<Time> latency) {
			if (data_age.from() == 0 && data_age.until() == 0) {
				data_age = latency;
			} else {
				data_age.widen(latency);
			}
		}

		long get_elapsed_time() const {
			return elapsed_time;
		}


		Interval<Time> get_data_age() const {
			return data_age;
		}
	};
}
#endif
