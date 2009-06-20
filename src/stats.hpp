#ifndef STATS_HPP_INCLUDED
#define STATS_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include "geometry.hpp"
#include "thread.hpp"
#include "wml_node_fwd.hpp"

#include <string>

namespace stats {

//download stats for a given level.
bool download(const std::string& lvl);

class manager {
public:
	manager();
	~manager();
private:
	threading::thread background_thread_;
};

class record;

typedef boost::shared_ptr<record> record_ptr;
typedef boost::shared_ptr<const record> const_record_ptr;

class record {
public:
	static record_ptr read(wml::const_node_ptr node);
	virtual ~record();
	virtual wml::node_ptr write() const = 0;
	virtual void draw() const = 0;
};

class die_record : public record {
public:
	explicit die_record(const point& p);
	wml::node_ptr write() const;
	void draw() const;
private:
	point p_;
};

void record_event(const std::string& lvl, const_record_ptr r);

}

#endif
