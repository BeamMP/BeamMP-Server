#include "Uuid.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>

std::string uuid::GenerateUuid() {
    static thread_local boost::uuids::random_generator Generator {};
    boost::uuids::uuid Id { Generator() };
    return boost::uuids::to_string(Id);
}
