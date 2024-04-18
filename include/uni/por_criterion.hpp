#ifndef POR_CRITERION_HPP
#define POR_CRITERION_HPP

#include "jobs.hpp"

namespace NP {

	namespace Uniproc {

		template<class Time> class POR_criterion
		{
		public:

			typedef std::vector<const Job<Time>*> Job_set;

			virtual const Job<Time>* select_job(Job_set& jobs) = 0;
		};

		template<class Time> class POR_priority_order: public POR_criterion<Time>
		{
		public:

			const Job<Time>* select_job(typename POR_criterion<Time>::Job_set& jobs) override
			{
				return *std::min_element(jobs.begin(), jobs.end(),
										 [](const Job<Time>* i, const Job<Time>* j) -> bool {
											 return i->higher_priority_than(*j);
										 });
			}
		};

		template<class Time> class POR_release_order: public POR_criterion<Time>
		{
		public:

			const Job<Time>* select_job(typename POR_criterion<Time>::Job_set& jobs) override
			{
				return *std::min_element(jobs.begin(), jobs.end(),
										 [](const Job<Time>* i, const Job<Time>* j) -> bool {
											 return i->earliest_arrival() < j->earliest_arrival();
										 });
			}
		};

	}
}

#endif