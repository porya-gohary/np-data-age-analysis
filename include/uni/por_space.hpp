#ifndef POR_SCHEDULE_SPACE_H
#define POR_SCHEDULE_SPACE_H

#include <unordered_map>
#include <map>
#include <vector>
#include <deque>
#include <list>
#include <algorithm>

#include <iostream>
#include <ostream>
#include <cassert>
#include <queue>

#include "config.h"
#include "problem.hpp"
#include "jobs.hpp"
#include "precedence.hpp"
#include "clock.hpp"

#include "uni/state.hpp"
#include "uni/space.hpp"
#include "uni/por_criterion.hpp"
#include "uni/reduction_set.hpp"

namespace NP {

	namespace Uniproc {

		template<class Time, class IIP = Null_IIP<Time>, class POR_criterion = POR_criterion<Time>>
		class Por_state_space : public State_space<Time, IIP> {

		public:

			typedef typename State_space<Time, IIP>::Problem Problem;
			typedef typename State_space<Time, IIP>::Workload Workload;
			typedef typename State_space<Time, IIP>::Abort_actions Abort_actions;
			typedef typename State_space<Time, IIP>::State State;
			typedef typename State_space<Time, IIP>::Job_precedence_set Job_precedence_set;

			static Por_state_space explore(
					const Problem &prob,
					const Analysis_options &opts) {
				// this is a uniprocessor analysis
				assert(prob.num_processors == 1);

				// Preprocess the job such that they release at or after their predecessors release
				// Alg.5 in the paper
				auto jobs = preprocess_jobs<Time>(prob.dag, prob.jobs);

				Por_state_space s = Por_state_space(jobs, prob.dag, prob.aborts,
													opts.timeout, opts.max_depth,
													opts.num_buckets, opts.early_exit);
				s.cpu_time.start();
				if (opts.be_naive)
					s.explore_naively();
				else
					s.explore();
				s.cpu_time.stop();
				return s;
			}

			// convenience interface for tests
			static Por_state_space explore_naively(const Workload &jobs) {
				Problem p{jobs};
				Analysis_options o;
				o.be_naive = true;
				return explore(p, o);
			}

			// convenience interface for tests
			static Por_state_space explore(const Workload &jobs) {
				Problem p{jobs};
				Analysis_options o;
				return explore(p, o);
			}

			unsigned long number_of_por_successes() const {
				return reduction_successes;
			}

			unsigned long number_of_por_failures() const {
				return reduction_failures;
			}

			std::vector<Reduction_set_statistics<Time>> get_reduction_set_statistics() const {
				return reduction_set_statistics;
			}

		protected:

			using State_space<Time, IIP>::explore;
			using State_space<Time, IIP>::explore_naively;
			using State_space<Time, IIP>::schedule;

			POR_criterion por_criterion;
			unsigned long reduction_successes, reduction_failures;
			std::vector<Reduction_set_statistics<Time>> reduction_set_statistics;
			std::vector<Job_precedence_set> job_precedence_sets;

			Por_state_space(const Workload &jobs,
							const Precedence_constraints &dag_edges,
							const Abort_actions &aborts,
							double max_cpu_time = 0,
							unsigned int max_depth = 0,
							std::size_t num_buckets = 1000,
							bool early_exit = true)
					: State_space<Time, IIP>(jobs, dag_edges, aborts, max_cpu_time, max_depth, num_buckets, early_exit),
					  por_criterion(), reduction_successes(0), reduction_failures(0), reduction_set_statistics(),
					  job_precedence_sets(jobs.size()) {
				for (auto e: dag_edges) {
					const Job<Time> &from = lookup<Time>(jobs, e.first);
					const Job<Time> &to = lookup<Time>(jobs, e.second);
					job_precedence_sets[index_of(to, jobs)].push_back(index_of(from, jobs));
				}

			}

			void schedule_eligible_successors_naively(const State &s, const Interval<Time> &next_range,
													  bool &found_at_least_one) override {
				typename Reduction_set<Time>::Job_set eligible_successors{};

				const Job<Time> *jp;
				foreach_possbly_pending_job_until(s, jp, next_range.upto()) {
						const Job<Time> &j = *jp;
						DM("+ " << j << std::endl);
						if (this->is_eligible_successor(s, j)) {
							DM("  --> can be next " << std::endl);
							eligible_successors.push_back(jp);
						}
					}

				if (eligible_successors.size() > 1) {
					Reduction_set<Time> reduction_set = create_reduction_set(s, eligible_successors);

					if (!reduction_set.has_potential_deadline_misses()) {
						DM("\n---\nPartial-order reduction is safe" << std::endl);
						schedule_naive(s, reduction_set);
						found_at_least_one = true;
						return;
					}
				}

				DM("\n---\nPartial-order reduction is not safe" << std::endl);
				for (const Job<Time> *j: eligible_successors) {
					this->schedule_job(s, *j);
					found_at_least_one = true;
				}
			}

			void schedule_eligible_successors(const State &s, const Interval<Time> &next_range,
											  bool &found_at_least_one) override {
				typename Reduction_set<Time>::Job_set eligible_successors{};

				const Job<Time> *jp;
				foreach_possbly_pending_job_until(s, jp, next_range.upto()) {
						const Job<Time> &j = *jp;
						DM("+ " << j << std::endl);
						if (this->is_eligible_successor(s, j)) {
							DM("  --> can be next " << std::endl);
							eligible_successors.push_back(jp);
						}
					}

				for (int i = 0; i < eligible_successors.size(); ++i) {
					DM("eligible_successors[" << i << "] = " << *eligible_successors[i] << std::endl);
				}

				if (eligible_successors.size() > 1) {
					// we have to check if reduction is safe
					Reduction_set<Time> reduction_set = create_reduction_set(s, eligible_successors);

					if (!reduction_set.has_potential_deadline_misses()) {
						DM("\n---\nPartial-order reduction is safe" << std::endl);
						schedule(s, reduction_set);
						found_at_least_one = true;
						return;
					}
				}

				DM("\n---\nPartial-order reduction is not safe" << std::endl);
				for (const Job<Time> *j: eligible_successors) {
					schedule(s, *j);
					found_at_least_one = true;
				}
			}

			void schedule(const State &s, const Reduction_set<Time> &reduction_set) {
				Interval<Time> finish_range = next_finish_times(reduction_set);

				auto k = s.next_key(reduction_set);

				auto r = this->states_by_key.equal_range(k);

				if (r.first != r.second) {
					Job_set sched_jobs{s.get_scheduled_jobs()};

					for (const Job<Time> *j: reduction_set.get_jobs()) {
						sched_jobs.add(this->index_of(*j));
					}

					for (auto it = r.first; it != r.second; it++) {
						State &found = *it->second;

						// key collision if the job sets don't match exactly
						if (found.get_scheduled_jobs() != sched_jobs)
							continue;

						// cannot merge without loss of accuracy if the
						// intervals do not overlap
						if (!finish_range.intersects(found.finish_range()))
							continue;

						// great, we found a match and can merge the states
						found.update_finish_range(finish_range);
						process_new_edge(s, found, reduction_set, finish_range);
						return;
					}
				}

				// If we reach here, we didn't find a match and need to create
				// a new state.

				std::vector<std::size_t> indices{};
				for (const Job<Time> *j: reduction_set.get_jobs()) {
					indices.push_back(this->index_of(*j));
				}

				const State &next =
						this->new_state(s, reduction_set, indices,
										finish_range,
										earliest_possible_job_release(s, reduction_set));
				//	DM("      -----> S" << (states.end() - states.begin()) << std::endl);
				process_new_edge(s, next, reduction_set, finish_range);
			}

			void schedule_naive(const State &s, const Reduction_set<Time> &reduction_set) {
				Interval<Time> finish_range = next_finish_times(reduction_set);

				std::vector<std::size_t> indices{};
				for (const Job<Time> *j: reduction_set.get_jobs()) {
					indices.push_back(this->index_of(*j));
				}

				const State &next =
						this->new_state(s, reduction_set, indices,
										finish_range,
										earliest_possible_job_release(s, reduction_set));
				//	DM("      -----> S" << (states.end() - states.begin()) << std::endl);
				process_new_edge(s, next, reduction_set, finish_range);
			}

			Reduction_set<Time>
			create_reduction_set(const State &s, typename Reduction_set<Time>::Job_set &eligible_successors) {
				std::vector<std::size_t> indices{};

				for (const Job<Time> *j: eligible_successors) {
					indices.push_back(this->index_of(*j));
				}

				Reduction_set<Time> reduction_set{Interval<Time>{s.earliest_finish_time(), s.latest_finish_time()},
												  eligible_successors, indices, job_precedence_sets};

				while (true) {
					if (reduction_set.has_potential_deadline_misses()) {
						reduction_failures++;
						reduction_set_statistics.push_back(Reduction_set_statistics<Time>{false, reduction_set});

						return reduction_set;
					}

					std::vector<const Job<Time> *> interfering_jobs{};

					const Job<Time> *jp;
					foreach_possbly_pending_job_until(s, jp, reduction_set.get_latest_busy_time() -
															 reduction_set.get_min_wcet()) {
							const Job<Time> &j = *jp;
							const Job_precedence_set &preds = this->job_precedence_sets[this->index_of(j)];
							if (reduction_set.can_interfere(j, preds, s.get_scheduled_jobs())) {
								interfering_jobs.push_back(jp);
							}
						}

					if (interfering_jobs.empty()) {
						reduction_successes++;
						reduction_set_statistics.push_back(Reduction_set_statistics<Time>{true, reduction_set});

						return reduction_set;
					} else {
						const Job<Time> *jx = por_criterion.select_job(interfering_jobs);
						reduction_set.add_job(jx, this->index_of(*jx));
					}
				}

			}


			Time earliest_possible_job_release(const State &s, const Reduction_set<Time> &ignored_set) {
				DM("      - looking for earliest possible job release starting from: "
						   << s.earliest_job_release() << std::endl);
				const Job<Time> *jp;
				foreach_possibly_pending_job(s, jp) {
						const Job<Time> &j = *jp;

						DM("         * looking at " << j << std::endl);

						// skip if it is the one we're ignoring
						bool ignored = false;
						for (const Job<Time> *ignored_job: ignored_set.get_jobs()) {
							if (&j == ignored_job) {
								ignored = true;
								break;
							}
						}

						if (!ignored) {
							DM("         * found it: " << j.earliest_arrival() << std::endl);
							// it's incomplete and not ignored => found the earliest
							return j.earliest_arrival();
						}
					}

				DM("         * No more future releases" << std::endl);
				return Time_model::constants<Time>::infinity();
			}

			Interval<Time> next_finish_times(const Reduction_set<Time> &reduction_set) {

				// standard case -- this job is never aborted or skipped
				return Interval<Time>{
						reduction_set.earliest_finish_time(),
						reduction_set.get_latest_busy_time()
				};
			}

			void process_new_edge(
					const State &from,
					const State &to,
					const Reduction_set<Time> &reduction_set,
					const Interval<Time> &finish_range) {
				// update response times
				for (const Job<Time> *j: reduction_set.get_jobs()) {
					this->update_finish_times(*j, Interval<Time>{reduction_set.earliest_finish_time(*j),
																 reduction_set.latest_finish_time(*j)});
				}
				// update statistics
				this->num_edges++;
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
				this->edges.push_back(std::make_unique<Reduced_edge>(reduction_set, &from, &to, finish_range));
#endif
			}


#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH

			struct Reduced_edge : State_space<Time, IIP>::Edge {
				Reduction_set<Time> reduction_set;

				Reduced_edge(const Reduction_set<Time> &reduction_set, const State *src, const State *tgt,
							 const Interval<Time> &fr)
						: State_space<Time, IIP>::Edge(reduction_set.get_jobs()[0], src, tgt, fr),
						  reduction_set{reduction_set} {
				}

				bool deadline_miss_possible() const override {
					return reduction_set.has_potential_deadline_misses();
				}

				Time earliest_finish_time() const override {
					return reduction_set.earliest_finish_time();
				}

				Time latest_finish_time() const override {
					return reduction_set.get_latest_busy_time();
				}

				Time earliest_start_time() const override {
					return reduction_set.earliest_start_time();
				}

				Time latest_start_time() const override {
					return reduction_set.latest_start_time();
				}

			};

			void print_edge(std::ostream &out, const std::unique_ptr<typename State_space<Time, IIP>::Edge> &e,
							unsigned int source_id, unsigned int target_id) const override {
				out << "\tS" << source_id
					<< " -> "
					<< "S" << target_id
					<< "[label=\"";

				auto r = dynamic_cast<Reduced_edge *>(e.get());
				if (r) {
					for (auto j: r->reduction_set.get_jobs()) {
						out << "T" << j->get_task_id()
							<< " J" << j->get_job_id()
							<< "\\nDL=" << j->get_deadline()
							<< "\\n";
					}
				} else {
					out << "T" << e->scheduled->get_task_id()
						<< " J" << e->scheduled->get_job_id()
						<< "\\nDL=" << e->scheduled->get_deadline();
				}

				out << "\\nES=" << e->earliest_start_time()
					<< "\\nLS=" << e->latest_start_time()
					<< "\\nEF=" << e->earliest_finish_time()
					<< "\\nLF=" << e->latest_finish_time()
					<< "\"";
				if (e->deadline_miss_possible()) {
					out << ",color=Red,fontcolor=Red";
				}
				out << ",fontsize=8" << "]"
					<< ";"
					<< std::endl;
				if (e->deadline_miss_possible()) {
					out << "S" << target_id
						<< "[color=Red];"
						<< std::endl;
				}
			}

#endif


		};

	}
}

#endif