/*
 * txedge.cpp -- process Bitcoin transaction inputs and outputs, create "edges"
 * (connecting all inputs and outputs, distributing values as weights)
 * 
 * Copyright 2018 Daniel Kondor <kondor.dani@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include "read_table.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <utility> //std::pair
#include <algorithm>
#include <stdexcept>


struct txrecord {
	unsigned int txid;
	int addr;
	int64_t value;
};

struct txedge {
	unsigned int txid;
	int addr_in;
	int addr_out;
	double w;
};


class txr_it {
	protected:
		read_table2 rt;
		const char* fn;
		txrecord r;
		int cskip;
		bool is_end_;
		uint64_t lines_max;
		uint64_t header_skip;
		//~ txr_it() = delete;
		
		// read next record from input
		int read_next() {
			if(!rt.read_line()) {
				if(rt.get_last_error() != T_EOF) return -1;
				is_end_ = true;
				return 0;
			}
			// first col: txid
			if(!rt.read_uint32(r.txid)) return -1;
			// skip cskip columns
			for(int i=0;i<cskip;i++) {
				int64_t tmp;
				if(!rt.read_int64(tmp)) return -1;
			}
			// read address -- only -1 is accepted as "unknown" address, other negative values are an error
			if(!rt.read_int32_limits(r.addr,-1,INT32_MAX)) return -1;
			// read value
			if(!rt.read_int64(r.value)) return -1;
			return 0;
		}
		// write error message and throw exception
		void handle_error() {
			fprintf(stderr,"txr_it: ");
			rt.write_error(stderr);
			is_end_ = true;
			throw new std::runtime_error("txr_it: invalid data!\n");
		}
		
	public:
		txr_it(FILE* in_, int cskip_, const char* fn_ = 0, uint64_t header_skip_ = 0, uint64_t lines_max_ = 0):rt(in_) {
			fn = fn_;
			header_skip = header_skip_;
			lines_max = lines_max_;
			cskip = cskip_;
			// read and ignore exactly the given number of header lines
			for(uint64_t j=0;j<header_skip;j++) rt.read_line(false);
			is_end_ = false;
			if(read_next()) handle_error();
		}
		
		
		txrecord operator *() const {
			if(is_end_) throw new std::runtime_error("txr_it(): iterator used after reaching the end!\n");
			return r;
		}
		const txrecord* operator ->() const {
			if(is_end_) throw new std::runtime_error("txr_it(): iterator used after reaching the end!\n");
			return &r;
		}
		void operator++() {
			if(read_next()) handle_error();
		}
		
		bool is_end() const {
			return is_end_;
		}
};


class tx {
	protected:
		std::vector<std::pair<int,int64_t> > inputs;
		std::vector<std::pair<int,int64_t> > outputs;
		unsigned int txid;
		txr_it& in;
		txr_it& out;
		//~ tx() = delete;
		
		static void vector_compress(std::vector<std::pair<int,int64_t> >& vec) {
			std::sort(vec.begin(),vec.end(),[](const auto& a, const auto& b) { return a.first < b.first; });
			
			size_t i=0;
			for(size_t j=1;j<vec.size();j++) {
				if(vec[i].first == vec[j].first) vec[i].second += vec[j].second;
				else {
					i++;
					if(i != j) vec[i] = vec[j];
				}
			}
			vec.erase(vec.begin()+i+1,vec.end());
		}
		
	public:
		tx(txr_it& txin_, txr_it& txout_):in(txin_),out(txout_) { }
		
		/* read next transaction (both inputs and outputs)
		 * return: true -- OK, false -- end of files
		 * throws exception on format error (from txr_it::operator++())
		 */
		bool read_next() {
			if(in.is_end() || out.is_end()) return false;
			
			inputs.clear();
			outputs.clear();
		
			// read txin first -- coinbase transactions have no inputs so they will be skipped
			txid = in->txid;
			for(;!in.is_end();++in) {
				if(in->txid != txid) break;
				inputs.push_back(std::make_pair(in->addr,in->value));
			}
			
			// check that txout matches or advance it
			for(;!out.is_end();++out) if(out->txid >= txid) break;
			
			if(out.is_end()) {
				// no outputs for the current transaction
				fprintf(stderr,"Warning: transaction %u has no outputs!\n",txid);
				return false;
			}
			if(out->txid > txid) {
				// found a transaction with > 0 inputs and no outputs, this should not happen
				fprintf(stderr,"Warning: transaction %u has no outputs!\n",txid);
				for(;!in.is_end();++in) {
					if(in->txid >= out->txid) break;
					if(in->txid > txid) {
						txid = in->txid;
						fprintf(stderr,"Warning: transaction %u has no outputs!\n",txid);
					}
				}
				// recursively try to find the inputs of the this transaction
				return read_next();
			}
			
			// add transaction outputs
			for(;!out.is_end();++out) {
				if(out->txid != txid) break;
				outputs.push_back(std::make_pair(out->addr,out->value));
			}
			
			
			// now we have a valid transaction with >0 inputs and outputs
			// sort inputs and outputs, merge if an address appears more than once
			vector_compress(inputs);
			vector_compress(outputs);
			return true;
		}
		
		// "iterator" to get all possible input address -> output address pairs
		// note: it does not correspond to the C++ iterator concept as it makes no sense to compare iterators of this kind
		struct iterator {
			protected:
				double sum;
				int txid;
				const std::vector<std::pair<int,int64_t> >& inputs;
				const std::vector<std::pair<int,int64_t> >& outputs;
				std::vector<std::pair<int,int64_t> >::const_iterator in_it;
				std::vector<std::pair<int,int64_t> >::const_iterator out_it;
				txedge e;
				
				void update_edge() {
					e.txid = txid;
					e.addr_in = in_it->first;
					e.addr_out = out_it->first;
					if(sum > 0.0) e.w = ((double)(in_it->second)) * (((double)(out_it->second)) / sum);
					else e.w = 0.0;
				}
			
			public:
				// check if the end has been reached (all possible edges processed)
				bool is_end() const { return in_it == inputs.cend(); }
				// select next edge, compute weight
				void operator ++() {
					if(in_it == inputs.cend()) return;
					out_it++;
					if(out_it == outputs.cend()) {
						in_it++;
						if(in_it == inputs.cend()) return;
						out_it = outputs.cbegin();
					}
					update_edge();
				}
				// access current edge
				const txedge& operator *() const {
					return e;
				}
				const txedge* operator ->() const {
					return &e;
				}
				
				iterator(const tx* t):inputs(t->inputs),outputs(t->outputs),txid(t->txid) {
					in_it = inputs.cbegin();
					out_it = outputs.cbegin();
					int64_t tmp = 0;
					for(auto it : inputs) tmp += it.second;
					sum = (double)tmp;
					if(in_it == inputs.cend()) return;
					if(out_it == outputs.cend()) { in_it = inputs.cend(); return; }
					update_edge();
				}
		};
		
		iterator get_iterator() const { return iterator(this); }
};


const char gzip[] = "/bin/gzip -cd";
const char xz[] = "/usr/bin/xz -cd";


FILE* open_pipe(const char* fn, const char* pr) {
	FILE* f = 0;
	char* tmp = (char*)malloc(sizeof(char)*(strlen(fn) + strlen(pr) + 4));
	if(!tmp) { fprintf(stderr,"open_pipe(): error allocating memory!\n"); return 0; }
	sprintf(tmp,"%s %s",pr,fn);
	f = popen(tmp,"r");
	if(!f) fprintf(stderr,"open_pipe(): error running command: %s\n",tmp);
	free(tmp);
	return f;
}

FILE* open_input(const char* fn, bool pgz, bool pxz) {
	if(pxz) return open_pipe(fn,xz);
	if(pgz) return open_pipe(fn,gzip);
	FILE* f = fopen(fn,"r");
	if(!f) fprintf(stderr,"open_input(): error opening file: %s\n",fn);
	return f;
}


int main(int argc, char **argv)
{
	char* txin = 0;
	char* txout = 0;
	
	bool in_gz = false;
	bool in_xz = false;
	bool out_gz = false;
	bool out_xz = false;
	
	bool old_format = false;
	
	for(int i=1;i<argc;i++) if(argv[i][0] == '-') switch(argv[i][1]) {
		case 'i':
			txin = argv[i+1];
			if(argv[i][2] == 'x') in_xz = true;
			if(argv[i][2] == 'z') in_gz = true;
			i++;
			break;
		case 'o':
			txout = argv[i+1];
			if(argv[i][2] == 'x') out_xz = true;
			if(argv[i][2] == 'z') out_gz = true;
			i++;
			break;
		case '1':
			old_format = true;
			break;
		default:
			fprintf(stderr,"Unknown command line argument: %s!\n",argv[i]);
			break;
	}
	
	if( !(txin && txout) ) {
		fprintf(stderr,"Error: missing input file names!\n");
		return 1;
	}
	FILE* in = 0;
	FILE* out = 0;
	
	// open transaction inputs file
	in = open_input(txin,in_gz,in_xz);
	out = open_input(txout,out_gz,out_xz);
	
	if(in && out) {
		txr_it in_it(in,old_format?1:3,txin);
		txr_it out_it(out,1,txout);
		
		tx tx_it(in_it,out_it);
		uint64_t txs = 0;
		uint64_t edges = 0;
		
		while(tx_it.read_next()) {
			txs++;
			for(tx::iterator it = tx_it.get_iterator();!it.is_end();++it) {
				edges++;
				fprintf(stdout,"%u\t%d\t%d\t%.17g\n",it->txid,it->addr_in,it->addr_out,it->w);
			}
		}
		fprintf(stderr,"%lu transactions matched, %lu edges generated\n",txs,edges);
	}
	else fprintf(stderr,"Error opening input files!\n");
	
	if(in) {
		if(in_gz || in_xz) pclose(in);
		else fclose(in);
	}
	if(out) {
		if(out_gz || out_xz) pclose(out);
		else fclose(out);
	}
	
	return 0;
}

