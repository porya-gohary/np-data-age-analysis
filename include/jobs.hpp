#ifndef JOBS_HPP
#define JOBS_HPP

#include <ostream>
#include <vector>
#include <algorithm> // for find
#include <functional> // for hash
#include <exception>

#include "time.hpp"
#include "interval.hpp"

namespace NP {

	typedef std::size_t hash_value_t;

	struct JobID {
		unsigned long job;
		unsigned long task;

		JobID(unsigned long j_id, unsigned long t_id)
				: job(j_id), task(t_id)
		{
		}

		bool operator==(const JobID& other) const
		{
			return this->task == other.task && this->job == other.job;
		}

		friend std::ostream& operator<< (std::ostream& stream, const JobID& id)
		{
			stream << "T" << id.task << "J" << id.job;
			return stream;
		}
	};

	template<class Time> class Job {

	public:
		typedef std::vector<Job<Time>> Job_set;
		typedef Time Priority; // Make it a time value to support EDF

	private:
		Interval<Time> arrival;
		Interval<Time> cost;
		Interval<Time> recovery_cost{0, 0};
		Time deadline;
		Priority priority;
		Priority recovery_priority;
		JobID id;
		unsigned int PE = 0;
		bool recovery_block = false;
		hash_value_t key;

		void compute_hash() {
			auto h = std::hash<Time>{};
			key = h(arrival.from());
			key = (key << 4) ^ h(id.task);
			key = (key << 4) ^ h(arrival.until());
			key = (key << 4) ^ h(cost.from());
			key = (key << 4) ^ h(deadline);
			key = (key << 4) ^ h(cost.upto());
			key = (key << 4) ^ h(id.job);
			key = (key << 4) ^ h(priority);
		}

	public:

		Job(unsigned long id,
			Interval<Time> arr, Interval<Time> cost,
			Time dl, Priority prio,
			unsigned long tid = 0)
				: arrival(arr), cost(cost),
				  deadline(dl), priority(prio), id(id, tid)
		{
			compute_hash();
		}

		Job(unsigned long id,
			Interval<Time> arr, Interval<Time> cost, Interval<Time> rec_cost,
			Time dl, Priority prio, Priority rec_prio,
			unsigned long tid = 0, unsigned int pe = 0)
				: arrival(arr), cost(cost), recovery_cost(rec_cost),
				  deadline(dl), priority(prio), recovery_priority(rec_prio), id(id, tid), PE(pe)
		{
			compute_hash();
		}

		hash_value_t get_key() const
		{
			return key;
		}

		Time earliest_arrival() const
		{
			return arrival.from();
		}

		Time latest_arrival() const
		{
			return arrival.until();
		}

		const Interval<Time>& arrival_window() const
		{
			return arrival;
		}

		void set_arrival(Interval<Time> arr)
		{
			arrival = arr;
		}

		Time least_cost() const
		{
			return cost.from();
		}

		Time maximal_cost() const
		{
			return cost.upto();
		}

		const Interval<Time>& get_cost() const
		{
			return cost;
		}

		bool recovery_block_status() const
		{
			return recovery_block;
		}

		void set_recovery_block_status(bool status)
		{
			recovery_block = status;
		}

		Time least_recovery_cost() const
		{
			return recovery_cost.from();
		}

		Time maximal_recovery_cost() const
		{
			return recovery_cost.upto();
		}

		const Interval<Time>& get_recovery_cost() const
		{
			return recovery_cost;
		}

		Priority get_priority() const
		{
			return priority;
		}

		void set_priority(Priority prio)
		{
			priority = prio;
		}

		Priority get_recovery_priority() const
		{
			return recovery_priority;
		}

		void set_recovery_priority(Priority rec_prio)
		{
			recovery_priority = rec_prio;
		}

		Time get_deadline() const
		{
			return deadline;
		}

		bool exceeds_deadline(Time t) const
		{
			return t > deadline
				   && (t - deadline) >
					  Time_model::constants<Time>::deadline_miss_tolerance();
		}

		JobID get_id() const
		{
			return id;
		}

		unsigned long get_job_id() const
		{
			return id.job;
		}

		unsigned long get_task_id() const
		{
			return id.task;
		}

		unsigned int get_pe() const
		{
			return PE;
		}

		bool is(const JobID& search_id) const
		{
			return this->id == search_id;
		}

		bool higher_priority_than(const Job &other, bool is_recovery_block = false, bool other_is_recovery_block = false) const
		{
			if (!is_recovery_block && !other_is_recovery_block)
				return priority < other.priority
					   // first tie-break by task ID
					   || (priority == other.priority
						   && id.task < other.id.task)
					   // second, tie-break by job ID
					   || (priority == other.priority
						   && id.task == other.id.task
						   && id.job < other.id.job);
			else if (is_recovery_block && !other_is_recovery_block)
				return recovery_priority < other.priority
					   // first tie-break by task ID
					   || (recovery_priority == other.priority
						   && id.task < other.id.task)
					   // second, tie-break by job ID
					   || (recovery_priority == other.priority
						   && id.task == other.id.task
						   && id.job < other.id.job);
			else if (!is_recovery_block && other_is_recovery_block)
				return priority < other.recovery_priority
					   // first tie-break by task ID
					   || (priority == other.recovery_priority
						   && id.task < other.id.task)
					   // second, tie-break by job ID
					   || (priority == other.recovery_priority
						   && id.task == other.id.task
						   && id.job < other.id.job);
			else
				return recovery_priority < other.recovery_priority
					   // first tie-break by task ID
					   || (recovery_priority == other.recovery_priority
						   && id.task < other.id.task)
					   // second, tie-break by job ID
					   || (recovery_priority == other.recovery_priority
						   && id.task == other.id.task
						   && id.job < other.id.job);
		}

		bool priority_at_least_that_of(const Job &other) const
		{
			return priority <= other.priority;
		}

		bool priority_exceeds(Priority prio_level) const
		{
			return priority < prio_level;
		}

		bool priority_at_least(Priority prio_level) const
		{
			return priority <= prio_level;
		}

		Interval<Time> scheduling_window() const
		{
			// inclusive interval, so take off one epsilon
			return Interval<Time>{
					earliest_arrival(),
					deadline - Time_model::constants<Time>::epsilon()};
		}

		static Interval<Time> scheduling_window(const Job& j)
		{
			return j.scheduling_window();
		}

		friend std::ostream& operator<< (std::ostream& stream, const Job& j)
		{
			stream << "Job{" << j.id.task << ", "  << j.id.job << ", " << j.arrival << ", "
				   << j.cost << ", " << j.recovery_cost << ", " << j.deadline << ", " << j.priority << "}";
			return stream;
		}

	};

	template<class Time>
	bool contains_job_with_id(const typename Job<Time>::Job_set& jobs,
							  const JobID& id)
	{
		auto pos = std::find_if(jobs.begin(), jobs.end(),
								[id] (const Job<Time>& j) { return j.is(id); } );
		return pos != jobs.end();
	}

	class InvalidJobReference : public std::exception
	{
	public:

		InvalidJobReference(const JobID& bad_id)
				: ref(bad_id)
		{}

		const JobID ref;

		virtual const char* what() const noexcept override
		{
			return "invalid job reference";
		}

	};

	template<class Time>
	const Job<Time>& lookup(const typename Job<Time>::Job_set& jobs,
							const JobID& id)
	{
		auto pos = std::find_if(jobs.begin(), jobs.end(),
								[id] (const Job<Time>& j) { return j.is(id); } );
		if (pos == jobs.end())
			throw InvalidJobReference(id);
		return *pos;
	}

}

namespace std {
	template<class T> struct hash<NP::Job<T>>
{
	std::size_t operator()(NP::Job<T> const& j) const
{
	return j.get_key();
}
};

template<> struct hash<NP::JobID>
{
	std::size_t operator()(NP::JobID const& id) const
	{
		hash<unsigned long> h;
		return (h(id.job) << 4) ^ h(id.task);
	}

};
}

#endif