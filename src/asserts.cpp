#include <stdio.h>

#include "asserts.hpp"

namespace {
	int throw_validation_failure = 0;
}

bool throw_validation_failure_on_assert()
{
	return throw_validation_failure != 0;
}

assert_recover_scope::assert_recover_scope()
{
	throw_validation_failure++;
}

assert_recover_scope::~assert_recover_scope()
{
	throw_validation_failure--;
}

void output_backtrace()
{
	//TODO: implement output of backtraces
}
