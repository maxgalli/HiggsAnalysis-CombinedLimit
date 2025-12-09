"""Timing profiler for Combine operations"""
import time
import functools
from collections import defaultdict


class TimingProfiler:
    """Simple timing profiler to track function execution times"""

    def __init__(self):
        self.timings = defaultdict(lambda: {"count": 0, "total_time": 0.0, "calls": []})
        self.call_stack = []
        self.enabled = True
        self.start_time = None
        self.end_time = None

    def time_function(self, func_name=None):
        """Decorator to time a function"""
        def decorator(func):
            name = func_name or f"{func.__module__}.{func.__name__}"

            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                if not self.enabled:
                    return func(*args, **kwargs)

                # Track overall start time
                if self.start_time is None:
                    self.start_time = time.time()

                start_time = time.time()
                self.call_stack.append(name)
                try:
                    result = func(*args, **kwargs)
                    return result
                finally:
                    elapsed = time.time() - start_time
                    self.timings[name]["count"] += 1
                    self.timings[name]["total_time"] += elapsed
                    self.timings[name]["calls"].append(elapsed)
                    self.call_stack.pop()

                    # Update end time
                    self.end_time = time.time()

            return wrapper
        return decorator

    def print_summary(self, min_time=0.001):
        """Print timing summary"""
        if not self.timings:
            print("\nNo timing data collected")
            return

        print("\n" + "=" * 80)
        print("TIMING PROFILER SUMMARY")
        print("=" * 80)

        # Calculate actual wall time
        if self.start_time and self.end_time:
            wall_time = self.end_time - self.start_time
        else:
            # Fallback: use the maximum single function time as proxy
            wall_time = max((t["total_time"] for _, t in self.timings.items()), default=0)

        # Sort by total time descending
        sorted_timings = sorted(
            self.timings.items(),
            key=lambda x: x[1]["total_time"],
            reverse=True
        )

        print(f"\nWall time: {wall_time:.4f}s")
        print(f"\n{'Function':<60} {'Calls':>8} {'Total (s)':>12} {'Avg (s)':>12} {'%':>6}")
        print("-" * 80)

        for name, data in sorted_timings:
            if data["total_time"] < min_time:
                continue

            avg_time = data["total_time"] / data["count"]
            pct = (data["total_time"] / wall_time * 100) if wall_time > 0 else 0

            # Shorten very long names
            display_name = name if len(name) <= 60 else "..." + name[-57:]

            print(f"{display_name:<60} {data['count']:>8} {data['total_time']:>12.4f} {avg_time:>12.4f} {pct:>5.1f}%")

        print("-" * 80)
        print(f"{'Measured wall time':<60} {' ':>8} {wall_time:>12.4f}")
        print("=" * 80)
        print("\nNote: Percentages are relative to wall time. Nested functions are included")
        print("      in their parent's time, so percentages will sum to more than 100%.")


# Global profiler instance
_profiler = TimingProfiler()


def get_profiler():
    """Get the global profiler instance"""
    return _profiler


def time_function(func_name=None):
    """Convenience decorator using global profiler"""
    return _profiler.time_function(func_name)


def print_timing_summary(min_time=0.001):
    """Print timing summary from global profiler"""
    _profiler.print_summary(min_time)
