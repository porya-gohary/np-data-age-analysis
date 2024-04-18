#ifndef TASK_HPP
#define TASK_HPP

#include "edge.hpp"
#include "interval.hpp"
#include <iostream>
// #include <iterator>
#include <vector>
#include <memory>
namespace NP {
	template<class Time>
	class Task {
	private:
		/* data */
		std::string name;
		unsigned long taskID;
		Time bcet = 0;
		Time wcet = 0;
		Time rec_cost_min = 0;
		Time rec_cost_max = 0;
		Time period = 0;
		Interval<Time> jitter;
		Time deadline = 0;
		unsigned int PE = 0;

		unsigned int current_job = 0;
		std::vector<std::shared_ptr<Edge<Task>>> rcv_edges;
		std::vector<std::shared_ptr<Edge<Task>>> snd_edges;


	public:
		typedef std::vector<std::shared_ptr<Task<Time>>> Task_set;

		Task() {

		}

		Task(unsigned long taskID, Time bcet, Time wcet, Time period, Time deadline, unsigned int PE) {
			this->bcet = bcet;
			this->wcet = wcet;
			this->period = period;
			this->jitter = Interval<Time>(0, 0);
			this->deadline = deadline;
			this->PE = PE;
			this->taskID = taskID;
			this->name = "T" + std::to_string(taskID);
		}

		Task(unsigned long taskID, Time bcet, Time wcet, Time rec_cost_min, Time rec_cost_max, Time period,
			 Time deadline, unsigned int PE) {
			this->bcet = bcet;
			this->wcet = wcet;
			this->rec_cost_min = rec_cost_min;
			this->rec_cost_max = rec_cost_max;
			this->period = period;
			this->jitter = Interval<Time>(0, 0);
			this->deadline = deadline;
			this->PE = PE;
			this->taskID = taskID;
			this->name = "T" + std::to_string(taskID);
		}

		//  Add a constructor for tasks with release jitter
		Task(unsigned long taskID, Time bcet, Time wcet, Time period, Interval<Time> jitter, Time deadline, unsigned int PE)
				: Task(taskID, bcet, wcet, period, deadline, PE) {
			this->jitter = jitter;
		}

		Task(unsigned long taskID, Time bcet, Time wcet, Time rec_cost_min, Time rec_cost_max, Time period, Interval<Time> jitter, Time deadline, unsigned int PE)
				: Task(taskID, bcet, wcet, rec_cost_min, rec_cost_max, period, deadline, PE) {
			this->jitter = jitter;
		}

		// Encapsulation
		void set_name(std::string name) {
			this->name = name;
			this->taskID = std::stol(name.substr(name.find("T") + 1));
		}

		void set_bcet(unsigned int bcet) {
			this->bcet = bcet;
		}

		void set_wcet(unsigned int wcet) {
			this->wcet = wcet;
		}

		void set_period(unsigned int period) {
			this->period = period;
		}

		void set_deadline(unsigned int deadline) {
			this->deadline = deadline;
		}

		void set_pe(unsigned int PE) {
			this->PE = PE;
		}

		std::string get_name() const {
			return name;
		}

		Time get_bcet() const {
			return bcet;
		}

		Time get_wcet() const {
			return wcet;
		}

		Time get_period() const {
			return period;
		}

		Time get_deadline() const {
			return deadline;
		}

		Time get_rec_cost_min() const {
			return rec_cost_min;
		}

		Time get_rec_cost_max() const {
			return rec_cost_max;
		}

		Interval<Time> get_rec_cost() const {
			return Interval<Time>(rec_cost_min, rec_cost_max);
		}

		unsigned int get_pe() const {
			return PE;
		}

		unsigned int get_cur_job() {
			return current_job;
		}

		void set_cur_job(unsigned int c) {
			this->current_job = c;
		}

		Time min_jitter() const {
			return jitter.from();
		}

		Time max_jitter() const {
			return jitter.until();
		}

		const Interval<Time> &jitter_window() const {
			return jitter;
		}

		void set_jitter_window(Interval<Time> jitter) {
			this->jitter = jitter;
		}


		std::vector<std::shared_ptr<Edge<Task>>> &get_rcv_edges() {
			return rcv_edges;
		}

		std::vector<std::shared_ptr<Edge<Task>>> &get_snd_edges() {
			return snd_edges;
		}

		std::vector<std::shared_ptr<Edge<Task>>> get_snd_edges() const {
			return snd_edges;
		}

		//Define functions

		void add_rcv_edges(std::shared_ptr<Edge<Task>> &e) {
			rcv_edges.push_back(e);
		}

		void add_snd_edges(std::shared_ptr<Edge<Task>> &e) {
			snd_edges.push_back(e);
		}

		unsigned long get_task_id() const {

			return taskID;
		}

		std::string print_spec() {
			std::string out = "";
			out = "Task name: " + name + "\n";
			out.append("\tBCET: " + std::to_string(bcet) + "\n");
			out.append("\tWCET: " + std::to_string(wcet) + "\n");
			out.append("\tRecovery Cost: I[" + std::to_string(rec_cost_min) + "," + std::to_string(rec_cost_max) + "]\n");
			out.append("\tPeriod: " + std::to_string(period) + "\n");
			out.append("\tJitter: I[" + std::to_string(jitter.from()) + "," + std::to_string(jitter.until()) + "]\n");
			out.append("\tDeadline: " + std::to_string(deadline) + "\n");
			out.append("\tPE: " + std::to_string(PE));
			return out;
		}

		bool is_ruunnable() {
			for (auto &e: rcv_edges) {
				if (e->get_src_task().get_cur_job() == 0) {
					return false;
				}
			}
			return true;
		}


		bool operator<(const Task &other) const {
			return name < other.name;
		}

		bool operator==(const Task &other) const {
			return name == other.name;
		}

	};
}
#endif