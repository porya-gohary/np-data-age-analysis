#ifndef DAG_HPP
#define DAG_HPP

#include <string>
#include <vector>
#include <list>
#include <memory>
#include <cmath>

#include "edge.hpp"
#include "task.hpp"

namespace NP {
	template<class Time>
	class dag {
	public:
		typedef std::vector<std::shared_ptr<Task<Time>>> Task_chain;
		typedef std::vector<Task_chain> Task_chains;
	private:
		std::vector<std::shared_ptr<Edge<Task<Time>>>>
				edges;
		std::vector<std::shared_ptr<Task<Time>>> tasks;
		long long hyperperiod = 0;
//    DAG name
		std::string name;
		Task_chains chains;

	public:
		dag() {

		}

		void add_task(std::shared_ptr<Task<Time>> &t) {
			tasks.push_back(t);
		}

		void add_task(unsigned long tid, Time bcet, Time wcet, Time period, Interval<Time> jitter, Time deadline,
					  unsigned int PE) {
			tasks.emplace_back(std::make_shared<Task<Time>>
									   (tid, bcet, wcet, period, jitter, deadline, PE));
		}

		void add_task(unsigned long tid, Time bcet, Time wcet, Time rec_cost_min, Time rec_cost_max, Time period, Interval<Time> jitter, Time deadline,
					  unsigned int PE) {
			tasks.emplace_back(std::make_shared<Task<Time>>
									   (tid, bcet, wcet, rec_cost_min, rec_cost_max, period, jitter, deadline, PE));
		}

		void add_edge(std::shared_ptr<Edge<Task<Time>>> &e) {
			edges.push_back(e);
		}

		void add_edge(unsigned long source_task, unsigned long destination_task) {

			auto srcT = find_task(source_task);
			auto desT = find_task(destination_task);

			edges.emplace_back(std::make_shared<Edge<Task<Time>>>(srcT, desT));
			srcT->add_snd_edges(edges.back());
			desT->add_rcv_edges(edges.back());

		}

		std::vector<std::shared_ptr<Task<Time>>> &get_tasks() {
			return tasks;
		}

		std::vector<std::shared_ptr<Edge<Task<Time>>>> &get_edges() {
			return edges;
		}

		void set_name(std::string name) {
			this->name = name;
		}

		std::string get_name() {
			return name;
		}

		std::vector<std::shared_ptr<Task<Time>>> get_mapped_task(unsigned int pe){
			// return all tasks mapped to a specific PE
			std::vector<std::shared_ptr<Task<Time>>> temp;
			for (auto &&i: tasks) {
				if (i->get_pe() == pe) {
					temp.push_back(i);
				}
			}
			return temp;
		}

		std::shared_ptr<Task<Time>> &find_task(unsigned long id) {
			for (auto &t: tasks) {
				if (t->get_task_id() == id)
					return t;
			}
			std::cerr << "Invalid TaskID => " << id << std::endl;
			exit(1);
		}

		void set_hyperperiod(Time h) {
			hyperperiod = h;
		}

		Time get_hyperperiod() {
			return hyperperiod;
		}


		// Recursive function to return gcd of a and b
		long long gcd(long long int a, long long int b) {
			if (b == 0)
				return a;
			return gcd(b, a % b);
		}

		// Function to return LCM of two numbers
		long long lcm(long long a, long long b) {
			return (a / gcd(a, b)) * b;
		}


		//Function to calculate hyperperiod
		void calculate_hyperperiod() {
			long long h = 1;
			for (auto &t_instance: tasks) {
				h = lcm(h, t_instance->get_period());
			}
			set_hyperperiod(h);
		}

		long long get_number_of_jobs() {
			long long nj = 0;
			for (auto &t: get_tasks()) {
				nj += get_hyperperiod() / t->get_period();
			}
			return nj;
		}

		std::vector<std::shared_ptr<Task<Time>>> get_source_tasks() {
			std::vector<std::shared_ptr<Task<Time>>> temp;
			for (auto &&i: tasks) {
				// std::cout <<"ROOT TASKS" <<std::endl;
				if (i->get_rcv_edges().size() == 0) {
					// std::cout<<i.getName() <<std::endl;
					temp.push_back(i);
				}

			}

			return temp;
		}


		std::vector<std::shared_ptr<Task<Time>>> get_sink_tasks() {
			std::vector<std::shared_ptr<Task<Time>>> temp;
			for (auto &&i: tasks) {
				// std::cout <<"SINK TASKS" <<std::endl;
				if (i->get_snd_edges().size() == 0) {
					// std::cout<<i.getName() <<std::endl;
					temp.push_back(i);
				}

			}

			return temp;
		}

		Task_chains find_all_paths(std::shared_ptr<Task<Time>> source, std::shared_ptr<Task<Time>> dist) {
			int s, d;
			Task_chains vec;
			s = source->get_task_id();
			d = dist->get_task_id();
			// Mark all the vertices as not visited
			bool *visited = new bool[tasks.size()];

			// Create an array to store paths
			int *path = new int[tasks.size()];
			int path_index = 0; // Initialize path[] as empty

			// Initialize all vertices as not visited
			for (int i = 0; i < tasks.size(); i++)
				visited[i] = false;

			// Call the recursive helper function to print all paths
			find_all_paths_util(s, d, visited, path, path_index, vec);
			return vec;
		}

		// A recursive function to find all paths from 'u' (label) to 'd' (label).
		// Visited[] keeps track of vertices in the current path.
		// Path[] stores actual vertices and path_index is current
		// index in path[]
		void find_all_paths_util(int u, int d, bool visited[],
								 int path[], int &path_index, Task_chains &vec) {
			// Mark the current node and store it in path[]
			visited[u] = true;
			path[path_index] = u;
			path_index++;

			// If current vertex is same as destination, then print
			// current path[]
			if (u == d) {
				Task_chain temp;
				for (int i = 0; i < path_index; i++) {
					temp.push_back(find_task(path[i]));
				}
				vec.push_back(temp);

			}


			// std::cout<<findTask("T"+std::to_string(u+1)).getSndEdges();
			for (auto &&j: find_task(u)->get_snd_edges()) {

				int i;
				i = j->get_dst_task()->get_task_id();
				if (!visited[i])
					find_all_paths_util(i, d, visited, path, path_index, vec);
			}


			// Remove current vertex from path[] and mark it as unvisited
			path_index--;
			visited[u] = false;
		}

		void add_task_chain(Task_chain &tc) {
			chains.push_back(tc);
		}

		void find_task_chains() {
			for (auto &t1: get_source_tasks()) {
				for (auto &t2: get_sink_tasks()) {
					Task_chains temp = find_all_paths(t1, t2);
					chains.insert(chains.end(), temp.begin(), temp.end());
				}

			}
		}

		void find_longest_task_chain() {
			Task_chain temp;
			for (auto &t1: get_source_tasks()) {
				for (auto &t2: get_sink_tasks()) {
					Task_chains temp1 = find_all_paths(t1, t2);
					if(!temp1.empty()) {
						if(temp.empty()) {
							temp = temp1[0];
						}
						for (auto &t: temp1) {
							if (t.size() > temp.size()) {
								temp = t;
							}
						}
					}
				}

			}
			add_task_chain(temp);
		}

		Task_chains &get_task_chains() {
			return chains;
		}

		std::size_t get_number_hp_observation_window() {
			std::size_t ow = 0;
			for (auto tc: get_task_chains()) {
				Time wct = 0;
				for (auto t: tc) {
					wct += 2 * t->get_period();
				}
				std::size_t owTemp = ceil((double) wct / get_hyperperiod());
				ow = owTemp > ow ? owTemp : ow;
			}
			return ow + 1;
		}

		Time get_chain_hyperperiod(std::size_t index) {
			long long h = 1;
			auto tc = chains[index];
			for (auto &t_instance: tc) {
				h = lcm(h, t_instance->get_period());
			}
			return h;
		}

		std::string to_string() {
			std::string out = "";
			out += "Input Tasks :\n";
			for (auto &&d: get_tasks()) {
				out += d->print_spec() + " \n";
			}

			out += "Edges : \n";
			for (auto &&e: get_edges()) {
				out += e->print_spec() + " \n";
			}
			out += "Task chains : \n";
			if (get_task_chains().size() == 0) {
				out += "No task chain find in Yaml file\n";
			} else {
				for (auto &&tc: get_task_chains()) {
					for (size_t i = 0; i < tc.size(); i++) {
						if (i < tc.size() - 1)
							out.append(tc[i]->get_name() + " => ");
						else
							out.append(tc[i]->get_name());
					}

					out.append("\n");
				}
			}
			return out;
		}


	};
}


#endif