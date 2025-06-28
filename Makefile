CC = gcc
CXX = g++
CFLAGS = -Wall -g -Iinclude
CXXFLAGS = -Wall -g -Iinclude -I/usr/local/include
LDLIBS = -lpthread
TLSLDLIBS = -lpthread -lssl -lcrypto
CXXLDLIBS = -lpthread -lprometheus-cpp-push -lprometheus-cpp-pull -lprometheus-cpp-core -lcurl -lz

SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj
WEB_DIR = src/app/web_dashboard
INSTALL_DIR = /opt/netkernel/web_dashboard

all: $(BIN_DIR)/http_server $(BIN_DIR)/dns_resolver $(BIN_DIR)/smtp_client $(BIN_DIR)/arp_sim $(BIN_DIR)/ethernet $(BIN_DIR)/prometheus_exporter $(BIN_DIR)/bgp_sim $(BIN_DIR)/icmp_diag $(BIN_DIR)/ipv6_stack $(BIN_DIR)/firewall $(BIN_DIR)/tls_openssl $(BIN_DIR)/tls_downgrade $(BIN_DIR)/mptcp $(BIN_DIR)/tcp_engine $(BIN_DIR)/udp_service install_web_dashboard

$(BIN_DIR)/http_server: $(OBJ_DIR)/app/http_server.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/dns_resolver: $(OBJ_DIR)/app/dns_resolver.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@

$(BIN_DIR)/smtp_client: $(OBJ_DIR)/app/smtp_client.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@

$(BIN_DIR)/arp_sim: $(OBJ_DIR)/app/arp_sim.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@

$(BIN_DIR)/ethernet: $(OBJ_DIR)/app/ethernet.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@

$(BIN_DIR)/prometheus_exporter: $(OBJ_DIR)/app/prometheus_exporter.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $^ -o $@ $(CXXLDLIBS)

$(BIN_DIR)/bgp_sim: $(OBJ_DIR)/app/bgp_sim.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/icmp_diag: $(OBJ_DIR)/app/icmp_diag.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/ipv6_stack: $(OBJ_DIR)/app/ipv6_stack.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/firewall: $(OBJ_DIR)/app/firewall.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/tls_openssl: $(OBJ_DIR)/app/tls_openssl.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(TLSLDLIBS)

$(BIN_DIR)/tls_downgrade: $(OBJ_DIR)/app/tls_downgrade.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(TLSLDLIBS)

$(BIN_DIR)/mptcp: $(OBJ_DIR)/app/mptcp.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/tcp_engine: $(OBJ_DIR)/app/tcp_engine.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/udp_service: $(OBJ_DIR)/app/udp_service.o
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/app/http_server.o: $(SRC_DIR)/app/http_server.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/dns_resolver.o: $(SRC_DIR)/app/dns_resolver.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/smtp_client.o: $(SRC_DIR)/app/smtp_client.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/arp_sim.o: $(SRC_DIR)/app/arp_sim.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/ethernet.o: $(SRC_DIR)/app/ethernet.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/prometheus_exporter.o: $(SRC_DIR)/app/prometheus_exporter.cpp
	@mkdir -p $(OBJ_DIR)/app
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/app/bgp_sim.o: $(SRC_DIR)/app/bgp_sim.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/icmp_diag.o: $(SRC_DIR)/app/icmp_diag.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/ipv6_stack.o: $(SRC_DIR)/app/ipv6_stack.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/firewall.o: $(SRC_DIR)/app/firewall.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/tls_openssl.o: $(SRC_DIR)/app/tls_openssl.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/tls_downgrade.o: $(SRC_DIR)/app/tls_downgrade.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/mptcp.o: $(SRC_DIR)/app/mptcp.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/tcp_engine.o: $(SRC_DIR)/app/tcp_engine.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/app/udp_service.o: $(SRC_DIR)/app/udp_service.c
	@mkdir -p $(OBJ_DIR)/app
	$(CC) $(CFLAGS) -c $< -o $@

install_web_dashboard:
	@mkdir -p $(INSTALL_DIR)
	cp $(WEB_DIR)/prometheus.yml $(INSTALL_DIR)/
	cp $(WEB_DIR)/index.html $(INSTALL_DIR)/

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) $(INSTALL_DIR)

.PHONY: all clean install_web_dashboard