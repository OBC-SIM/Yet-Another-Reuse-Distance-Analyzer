#include "runtime_measurement.hpp"

#include <chrono>
#include <ctime>
#include <limits>
#include <stdexcept>

#if defined(__linux__)
#include <sys/resource.h>
#include <sys/utsname.h>
#endif

namespace yarda::detail
{
namespace
{

#if defined(__linux__)
std::uint64_t monotonic_time_ns()
{
  const auto duration = std::chrono::steady_clock::now().time_since_epoch();
  const auto count =
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  if (count < 0)
    throw std::runtime_error("steady clock has a negative timestamp");
  return static_cast<std::uint64_t>(count);
}

std::uint64_t process_peak_rss_bytes()
{
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0)
    throw std::runtime_error("cannot measure process peak RSS");
  return linux_peak_rss_bytes(usage.ru_maxrss);
}

AnalysisTelemetryHost process_host()
{
  utsname host{};
  if (uname(&host) != 0)
    throw std::runtime_error("cannot read measurement host");
  return {host.nodename, host.sysname, host.release, host.machine};
}

std::string measured_at_utc()
{
  const auto now = std::time(nullptr);
  std::tm utc{};
  char timestamp[21]{};
  if (now == std::time_t{-1} || !gmtime_r(&now, &utc) ||
      std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc) !=
          20)
    throw std::runtime_error("cannot read UTC measurement timestamp");
  return timestamp;
}
#endif

} // namespace

std::uint64_t linux_peak_rss_bytes(std::int64_t kibibytes)
{
  if (kibibytes < 0) throw std::invalid_argument("Linux peak RSS is negative");
  const auto count = static_cast<std::uint64_t>(kibibytes);
  if (count > std::numeric_limits<std::uint64_t>::max() / 1024)
    throw std::overflow_error("Linux peak RSS overflows uint64_t bytes");
  return count * 1024;
}

AnalysisTelemetryProviders runtime_measurement_providers()
{
#if defined(__linux__)
  return {monotonic_time_ns, process_peak_rss_bytes, process_host,
          measured_at_utc};
#else
  throw std::runtime_error("default hierarchy telemetry requires Linux");
#endif
}

} // namespace yarda::detail
