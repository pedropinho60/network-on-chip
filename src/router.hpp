// noc_systemc.cpp
#include <systemc>
#include <iostream>

using namespace sc_core;
using namespace std;

// Define directions
enum Direction {NORTH = 0, SOUTH = 1, EAST = 2, WEST = 3, LOCAL = 4};
enum RoutingAlgorithm {XY = 0, RANDOM = 1};

struct Packet {
    int src_x, src_y;
    int dst_x, dst_y;
    int payload;
};

std::ostream& operator<<(std::ostream& os, const Packet& pkt) {
    os << "[src=(" << pkt.src_x << "," << pkt.src_y << ")"
       << " dst=(" << pkt.dst_x << "," << pkt.dst_y << ")"
       << " payload=" << pkt.payload << "]";
    return os;
}

SC_MODULE(Node) {
    sc_fifo_out<Packet> out;
    sc_fifo_in<Packet> in;
    int x, y;

    void send_packets() {
        wait(5, SC_NS);
        Packet pkt = {x, y, rand() % 4, rand() % 4, 42};
        cout << sc_time_stamp() << ": Node(" << x << "," << y << ") sends packet to node(" << pkt.dst_x << "," << pkt.dst_y << ")" << endl;
        out.write(pkt);
    }

    void consume_packets() {
        while (true) {
            Packet pkt = in.read();
            cout << sc_time_stamp() << ": Node(" << x << "," << y << ") received packet from node(" << pkt.src_x << "," << pkt.src_y << ")" << endl;
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
    int x, y;

    void process() {
        while (true) {
            for (int i = 0; i < 5; ++i) {
                if (in[i].num_available() > 0) {
                    Packet pkt = in[i].read();
                    int dir = route(pkt);
                    wait(1, SC_NS);
                    out[dir].write(pkt);
                    break;
                }
            }
        }
    }

    int route(Packet pkt) {
        if (algorithm == XY) {
            if (pkt.dst_x > x) return EAST;
            if (pkt.dst_x < x) return WEST;
            if (pkt.dst_y > y) return SOUTH;
            if (pkt.dst_y < y) return NORTH;
            return LOCAL;
        } else {
            std::vector<int> options;
            if (pkt.dst_x > x) options.push_back(EAST);
            if (pkt.dst_x < x) options.push_back(WEST);
            if (pkt.dst_y > y) options.push_back(SOUTH);
            if (pkt.dst_y < y) options.push_back(NORTH);
            if (pkt.dst_x == x && pkt.dst_y == y) return LOCAL;
            return options[rand() % options.size()];
        }
    }

    SC_CTOR(Router) {
        SC_THREAD(process);
    }
};

