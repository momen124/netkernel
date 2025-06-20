/* prometheus_exporter.cpp: A Prometheus exporter that exposes metrics about netkernel
 * tools (e.g., HTTP requests, DNS lookups) via an HTTP endpoint (localhost:9091/metrics).
 * It uses prometheus-cpp to define counters and gauges, serving them for Prometheus to
 * scrape. Like a librarian posting library activity tallies on a bulletin board for
 * monitoring. */

/* Include standard libraries and prometheus-cpp for metrics */
#include <prometheus/exposer.h>      /* For Exposer (HTTP server for metrics) */
#include <prometheus/registry.h>     /* For Registry (metric storage) */
#include <prometheus/counter.h>      /* For Counter metrics */
#include <prometheus/gauge.h>        /* For Gauge metrics */
#include <iostream>                  /* For std::cout (printing) */
#include <memory>                    /* For std::shared_ptr (smart pointers) */
#include <thread>                    /* For std::this_thread::sleep_for */
#include <chrono>                    /* For std::chrono::seconds */

/* Main function: Entry point of the Prometheus exporter */
int main() {
    /* Create an exposer to serve metrics on localhost:9091/metrics */
    prometheus::Exposer exposer{"127.0.0.1:9091"};
    std::cout << "Prometheus exporter running at http://localhost:9091/metrics\n";

    /* Create a registry to store metrics (shared pointer for automatic memory management) */
    auto registry = std::make_shared<prometheus::Registry>();

    /* Define a counter for HTTP requests (from http_server.c) */
    auto& http_requests_counter = prometheus::BuildCounter()
        .Name("http_requests_total")
        .Help("Total number of HTTP requests processed")
        .Register(*registry)
        .Add({{"method", "GET"}}); /* Example label for GET requests */

    /* Define a counter for DNS lookups (from dns_resolver.c) */
    auto& dns_lookups_counter = prometheus::BuildCounter()
        .Name("dns_lookups_total")
        .Help("Total number of DNS lookups performed")
        .Register(*registry)
        .Add({});

    /* Define a gauge for active HTTP connections */
    auto& active_connections_gauge = prometheus::BuildGauge()
        .Name("active_connections")
        .Help("Number of active HTTP connections")
        .Register(*registry)
        .Add({});

    /* Attach the registry to the exposer */
    exposer.RegisterCollectable(registry);

    /* Simulate metric updates (in a real setup, instrument http_server.c, dns_resolver.c) */
    while (true) {
        /* Increment HTTP requests (e.g., simulate GET requests) */
        http_requests_counter.Increment();
        std::cout << "Incremented http_requests_total\n";

        /* Increment DNS lookups (e.g., simulate a lookup) */
        dns_lookups_counter.Increment();
        std::cout << "Incremented dns_lookups_total\n";

        /* Update active connections (e.g., simulate 5 connections) */
        active_connections_gauge.Set(5);
        std::cout << "Set active_connections to 5\n";

        /* Sleep for 5 seconds to simulate periodic updates */
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}