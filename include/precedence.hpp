#ifndef PRECEDENCE_HPP
#define PRECEDENCE_HPP

#include <queue>
#include <unordered_map>

#include "jobs.hpp"
#include "index_set.hpp"

namespace NP {

	typedef std::pair<JobID, JobID> Precedence_constraint;
	typedef std::vector<Precedence_constraint> Precedence_constraints;
	typedef std::vector<std::size_t> Job_precedence_set;

	template<class Time>
	void validate_prec_refs(const Precedence_constraints &dag,
							const typename Job<Time>::Job_set jobs) {
		for (auto constraint: dag) {
			lookup<Time>(jobs, constraint.first);
			lookup<Time>(jobs, constraint.second);
		}
	}

	template<class Time>
	typename Job<Time>::Job_set topological_sort(const Precedence_constraints &dag,
												 const typename Job<Time>::Job_set &jobs) {
		std::vector<Job_precedence_set> job_precedence_sets(jobs.size());

		for (auto e: dag) {
			const Job<Time> &from = lookup<Time>(jobs, e.first);
			const Job<Time> &to = lookup<Time>(jobs, e.second);
			job_precedence_sets[index_of(to, jobs)].push_back(index_of(from, jobs));
		}

		return topological_sort<Time>(job_precedence_sets, jobs);
	}

	template<class Time>
	typename Job<Time>::Job_set topological_sort(const std::vector<Job_precedence_set> &job_precedence_sets,
												 const std::vector<const Job<Time> *> &jobs) {
		typename Job<Time>::Job_set job_refs{};

		for (auto j: jobs) {
			job_refs.push_back(*j);
		}

		return topological_sort<Time>(job_precedence_sets, job_refs);
	}

	template<class Time>
	typename Job<Time>::Job_set topological_sort(const std::vector<Job_precedence_set> &job_precedence_sets,
												 const typename Job<Time>::Job_set &jobs) {
		std::unordered_map<JobID, std::size_t> job_index_map{};

		for (auto &j: jobs) {
			job_index_map.emplace(j.get_id(), index_of(j, jobs));
		}

		typename std::vector<std::size_t> sorted_indices{};
		Index_set processed_indices{};
		typename Job<Time>::Job_set temp{};
		typename Job<Time>::Job_set sorted_jobs{};

		for (auto j: jobs) {
			size_t idx = job_index_map.find(j.get_id())->second;
			const Job_precedence_set &preds = job_precedence_sets[idx];

			if (preds.empty()) {
				sorted_indices.push_back(idx);
				processed_indices.add(idx);
				sorted_jobs.push_back(j);
			} else {
				temp.push_back(j);
			}
		}

		std::deque<Job<Time>> queue(temp.begin(), temp.end());

		while (not queue.empty()) {
			Job<Time> &j = queue.front();
			queue.pop_front();
			size_t idx = job_index_map.find(j.get_id())->second;
			const Job_precedence_set &preds = job_precedence_sets[idx];

			// All predecessors of j have been sorted already
			if (processed_indices.includes(preds)) {
				sorted_indices.push_back(idx);
				processed_indices.add(idx);
				sorted_jobs.push_back(j);
			} else {
				queue.push_back(j);
			}
		}

		return sorted_jobs;
	}

	template<class Time>
	typename Job<Time>::Job_set set_arrival_times(const std::vector<Job_precedence_set> &job_precedence_sets,
												  typename Job<Time>::Job_set jobs) {
		std::unordered_map<JobID, std::size_t> job_index_map{};

		for (auto &j: jobs) {
			job_index_map.emplace(j.get_id(), index_of(j, jobs));
		}

		for (auto &j: jobs) {
			size_t idx = job_index_map.find(j.get_id())->second;
			const Job_precedence_set &preds = job_precedence_sets[idx];

			// get all ancestors of j and add them to a vector
			Job_precedence_set ancestors = preds;
			std::queue<size_t> q;
			for (auto pred_idx: preds) {
				q.push(pred_idx);
			}
			while (!q.empty()) {
				size_t pred_idx = q.front();
				q.pop();
				const Job_precedence_set &pred_preds = job_precedence_sets[pred_idx];
				for (auto pred_pred_idx: pred_preds) {
					q.push(pred_pred_idx);
					ancestors.push_back(pred_pred_idx);
				}
			}

			Time max_earliest_arrival_time = j.earliest_arrival();
			Time max_latest_arrival_time = j.latest_arrival();
			if (!ancestors.empty()) {
				for (auto pred_idx: ancestors) {
					const Job<Time> &pred = jobs[pred_idx];
					max_earliest_arrival_time = std::max(max_earliest_arrival_time, pred.earliest_arrival());
					max_latest_arrival_time = std::max(max_latest_arrival_time, pred.latest_arrival());
				}
			}
			j.set_arrival(Interval<Time>(max_earliest_arrival_time, max_latest_arrival_time));
		}
		return jobs;

	}

	template<class Time>
	std::size_t index_of(const Job<Time> &j, const typename Job<Time>::Job_set &jobs) {
		return (std::size_t) (&j - &(jobs[0]));
	}

	template<class Time>
	typename Job<Time>::Job_set preprocess_jobs(const Precedence_constraints &dag,
												const typename Job<Time>::Job_set &jobs) {
		std::vector<Job_precedence_set> job_precedence_sets(jobs.size());

		for (auto e: dag) {
			const Job<Time> &from = lookup<Time>(jobs, e.first);
			const Job<Time> &to = lookup<Time>(jobs, e.second);
			job_precedence_sets[index_of(to, jobs)].push_back(index_of(from, jobs));
		}

		return topological_sort<Time>(job_precedence_sets, set_arrival_times<Time>(job_precedence_sets, jobs));
	}

}

#endif