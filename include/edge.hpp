#ifndef EDGE_HPP
#define EDGE_HPP

#include <string>
#include <memory>
namespace NP {
	template<class Task>
	class Edge {
	private:
		std::shared_ptr<Task> src_task;
		std::shared_ptr<Task> dst_task;
		std::string name;

	public:
		Edge(std::shared_ptr<Task> &srcT, std::shared_ptr<Task> &dstT, std::string name)//:srcTask(srcT) , dstTask(dstT)
		{
			this->src_task = srcT;
			this->dst_task = dstT;
			this->name = name;

		}

		Edge(std::shared_ptr<Task> &srcT, std::shared_ptr<Task> &dstT)//:srcTask(srcT) , dstTask(dstT)
		{
			this->src_task = srcT;
			this->dst_task = dstT;
			this->name = "E" + std::to_string(srcT->get_task_id()) + std::to_string(dstT->get_task_id());
		}

		// Encapsulation

		void set_src_task(std::shared_ptr<Task> &t) {
			src_task = t;
		}

		void set_dst_task(std::shared_ptr<Task> &t) {
			dst_task = t;
		}

		void set_name(std::string n) {
			name = n;
		}

		std::shared_ptr<Task> &get_src_task() {
			return src_task;
		}

		std::shared_ptr<Task> &get_dst_task() {
			return dst_task;
		}

		std::string get_name() {
			return name;
		}


		bool operator<(const Edge &other) const {
			return name < other.name;
		}

		bool operator=(const Edge &other) const {
			return name == other.name;
		}

		std::string print_spec() {
			std::string out = "";
			out = "\t" + get_name() + ": " + get_src_task()->get_name()
				  + " --> " + get_dst_task()->get_name();
			return out;
		}

	};
}
#endif