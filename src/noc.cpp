#include <systemc>
#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace sc_core;
using namespace std;

#define ALGORITHM XY

enum Direction {NORTH = 0, SOUTH = 1, EAST = 2, WEST = 3, LOCAL = 4};
enum RoutingAlgorithm {XY = 0, WEST_FIRST = 1};

struct Packet {
    int src_i, src_j;
    int dst_i, dst_j;
    int payload;
};

ostream& operator<<(ostream& os, const Packet& pkt) {
    os << "[src=(" << pkt.src_i << "," << pkt.src_j << ")"
       << " dst=(" << pkt.dst_i << "," << pkt.dst_j << ")"
       << " payload=" << pkt.payload << "]";
    return os;
}

SC_MODULE(Node) {
    sc_fifo_out<Packet> out;
    sc_fifo_in<Packet> in;
    int i, j;

    void send_packets() {
        wait(100, SC_NS);
        bool flag = false;
        Packet pkt;
        
        while (!flag) {
            pkt = {i, j, rand() % 4, rand() % 4, rand() % 100};
            if (pkt.dst_i != i || pkt.dst_j != j) flag = true;
        }
        
        cout << sc_time_stamp() << ": Node(" << i << "," << j
                << ") sends " << pkt << endl;
        out.write(pkt);
    }

    void consume_packets() {
        while (true) {
            Packet pkt = in.read();
            cout << sc_time_stamp() << ": Node(" << i << "," << j
                    << ") received " << pkt << endl;
        }
    }

    SC_CTOR(Node) {
        SC_THREAD(send_packets);
        SC_THREAD(consume_packets);
    }
};

SC_MODULE(Router) {
    RoutingAlgorithm algorithm;
    sc_fifo_in<Packet> in[5];
    sc_fifo_out<Packet> out[5];
    int i, j;

    int direction = 0;

    void process() {
        while (true) {
            for(int i = 0; i < 5; i++) {
                int current_dir = (direction + i) % 5;
                if (in[current_dir].num_available() > 0) {
                    Packet pkt = in[current_dir].read();
                    int dir = route(pkt);
                    wait(1, SC_NS);
                    out[dir].write(pkt);
                    direction = (current_dir + 1) % 5;
                    break;
                }
            }
            wait(1, SC_NS);
        }
    }

    int route(Packet pkt) {
        if (algorithm == XY) {
            if (pkt.dst_j > j) return EAST;
            if (pkt.dst_j < j) return WEST;
            if (pkt.dst_i > i) return SOUTH;
            if (pkt.dst_i < i) return NORTH;
            return LOCAL;
        } else {
            if (pkt.dst_j < j) return WEST;
            if (pkt.dst_i < i) return NORTH;
            if (pkt.dst_i > i) return SOUTH;
            if (pkt.dst_j > j) return EAST;
            return LOCAL;
        }
    }

    SC_CTOR(Router) {
        SC_THREAD(process);
    }
};

SC_MODULE(Network) {
    const static int N = 4;
    Router* routers[N][N];
    Node* nodes[N][N];

    sc_fifo<Packet>* channel_router_to_node[N][N];
    sc_fifo<Packet>* channel_node_to_router[N][N];

    // Setas para cima
    sc_fifo<Packet>* channel_north[N][N];
    // Setas para baixo
    sc_fifo<Packet>* channel_south[N][N];
    // Setas para direita
    sc_fifo<Packet>* channel_east[N][N];
    // Setas para esquerda
    sc_fifo<Packet>* channel_west[N][N];

    sc_fifo<Packet>* channel_dummy_north[N][2];
    sc_fifo<Packet>* channel_dummy_south[N][2];
    sc_fifo<Packet>* channel_dummy_east[N][2];
    sc_fifo<Packet>* channel_dummy_west[N][2];

    SC_CTOR(Network) {
        for (int i = 0; i < N; ++i) {
            channel_dummy_north[i][0] = new sc_fifo<Packet>(1);
            channel_dummy_north[i][1] = new sc_fifo<Packet>(1);
            channel_dummy_south[i][0] = new sc_fifo<Packet>(1);
            channel_dummy_south[i][1] = new sc_fifo<Packet>(1);
            channel_dummy_east[i][0] = new sc_fifo<Packet>(1);
            channel_dummy_east[i][1] = new sc_fifo<Packet>(1);
            channel_dummy_west[i][0] = new sc_fifo<Packet>(1);
            channel_dummy_west[i][1] = new sc_fifo<Packet>(1);

            for (int j = 0; j < N; ++j) {
                string rname = "R_" + to_string(i) + "_" + to_string(j);
                routers[i][j] = new Router(rname.c_str());
                routers[i][j]->i = i;
                routers[i][j]->j = j;
                routers[i][j]->algorithm = ALGORITHM;

                string nname = "N_" + to_string(i) + "_" + to_string(j);
                nodes[i][j] = new Node(nname.c_str());
                nodes[i][j]->i = i;
                nodes[i][j]->j = j;

                channel_router_to_node[i][j] = new sc_fifo<Packet>(1);
                channel_node_to_router[i][j] = new sc_fifo<Packet>(1);
                channel_north[i][j] = new sc_fifo<Packet>(1);
                channel_south[i][j] = new sc_fifo<Packet>(1);
                channel_east[i][j]  = new sc_fifo<Packet>(1);
                channel_west[i][j]  = new sc_fifo<Packet>(1);

                routers[i][j]->in[LOCAL](*channel_node_to_router[i][j]);
                routers[i][j]->out[LOCAL](*channel_router_to_node[i][j]);
                nodes[i][j]->in(*channel_router_to_node[i][j]);
                nodes[i][j]->out(*channel_node_to_router[i][j]);
            }
        }

        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if (i > 0) {
                    routers[i][j]->in[NORTH](*channel_south[i - 1][j]);
                    routers[i - 1][j]->out[SOUTH](*channel_south[i - 1][j]);
                } else {
                    routers[i][j]->in[NORTH](*channel_dummy_north[j][0]);
                    routers[i][j]->out[NORTH](*channel_dummy_north[j][1]);
                }

                if (i < N - 1) {
                    routers[i][j]->in[SOUTH](*channel_north[i + 1][j]);
                    routers[i + 1][j]->out[NORTH](*channel_north[i + 1][j]);
                } else {
                    routers[i][j]->in[SOUTH](*channel_dummy_south[j][0]);
                    routers[i][j]->out[SOUTH](*channel_dummy_south[j][1]);
                }

                if (j > 0) {
                    routers[i][j]->in[WEST](*channel_east[i][j - 1]);
                    routers[i][j - 1]->out[EAST](*channel_east[i][j - 1]);
                } else {
                    routers[i][j]->in[WEST](*channel_dummy_west[i][0]);
                    routers[i][j]->out[WEST](*channel_dummy_west[i][1]);
                }

                if (j < N - 1) {
                    routers[i][j]->in[EAST](*channel_west[i][j + 1]);
                    routers[i][j + 1]->out[WEST](*channel_west[i][j + 1]);
                } else {
                    routers[i][j]->in[EAST](*channel_dummy_east[i][0]);
                    routers[i][j]->out[EAST](*channel_dummy_east[i][1]);
                }
            }
        }
    };
};

int sc_main(int argc, char* argv[]) {
    srand(time(NULL));

    Network network("network");

    sc_start(10, SC_SEC);
    return 0;
}
