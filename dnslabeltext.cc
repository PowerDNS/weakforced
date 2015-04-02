
#line 1 "dnslabeltext.rl"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include "namespaces.hh"

namespace {
void appendSplit(vector<string>& ret, string& segment, char c)
{
  if(segment.size()>254) {
    ret.push_back(segment);
    segment.clear();
  }
  segment.append(1, c);
}

}

vector<string> segmentDNSText(const string& input )
{

#line 26 "dnslabeltext.cc"
static const char _dnstext_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 2, 0, 1, 
	2, 4, 5
};

static const char _dnstext_key_offsets[] = {
	0, 0, 1, 3, 5, 7, 9, 11, 
	15
};

static const unsigned char _dnstext_trans_keys[] = {
	34u, 34u, 92u, 48u, 57u, 48u, 57u, 48u, 
	57u, 34u, 92u, 32u, 34u, 9u, 13u, 34u, 
	0
};

static const char _dnstext_single_lengths[] = {
	0, 1, 2, 0, 0, 0, 2, 2, 
	1
};

static const char _dnstext_range_lengths[] = {
	0, 0, 0, 1, 1, 1, 0, 1, 
	0
};

static const char _dnstext_index_offsets[] = {
	0, 0, 2, 5, 7, 9, 11, 14, 
	18
};

static const char _dnstext_trans_targs[] = {
	2, 0, 7, 3, 2, 4, 2, 5, 
	0, 6, 0, 7, 3, 2, 8, 2, 
	8, 0, 2, 0, 0
};

static const char _dnstext_trans_actions[] = {
	3, 0, 0, 0, 11, 7, 5, 7, 
	0, 7, 0, 9, 9, 16, 0, 13, 
	0, 0, 13, 0, 0
};

static const char _dnstext_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 1, 
	1
};

static const int dnstext_start = 1;
static const int dnstext_first_final = 7;
static const int dnstext_error = 0;

static const int dnstext_en_main = 1;


#line 26 "dnslabeltext.rl"

	(void)dnstext_error;  // silence warnings
	(void)dnstext_en_main;
        const char *p = input.c_str(), *pe = input.c_str() + input.length();
        const char* eof = pe;
        int cs;
        char val = 0;

        string segment;
        vector<string> ret;

        
#line 96 "dnslabeltext.cc"
	{
	cs = dnstext_start;
	}

#line 101 "dnslabeltext.cc"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _dnstext_trans_keys + _dnstext_key_offsets[cs];
	_trans = _dnstext_index_offsets[cs];

	_klen = _dnstext_single_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _dnstext_range_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	cs = _dnstext_trans_targs[_trans];

	if ( _dnstext_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _dnstext_actions + _dnstext_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 38 "dnslabeltext.rl"
	{ 
                        ret.push_back(segment);
                        segment.clear();
                }
	break;
	case 1:
#line 42 "dnslabeltext.rl"
	{ 
                        segment.clear();
                }
	break;
	case 2:
#line 46 "dnslabeltext.rl"
	{
                  char c = *p;
                  appendSplit(ret, segment, c);
                }
	break;
	case 3:
#line 50 "dnslabeltext.rl"
	{
                  char c = *p;
                  val *= 10;
                  val += c-'0';
                  
                }
	break;
	case 4:
#line 56 "dnslabeltext.rl"
	{
                  appendSplit(ret, segment, val);
                  val=0;
                }
	break;
	case 5:
#line 61 "dnslabeltext.rl"
	{
                  appendSplit(ret, segment, *(p));
                }
	break;
#line 216 "dnslabeltext.cc"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _dnstext_actions + _dnstext_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 38 "dnslabeltext.rl"
	{ 
                        ret.push_back(segment);
                        segment.clear();
                }
	break;
#line 239 "dnslabeltext.cc"
		}
	}
	}

	_out: {}
	}

#line 74 "dnslabeltext.rl"


        if ( cs < dnstext_first_final ) {
                throw runtime_error("Unable to parse DNS TXT '"+input+"'");
        }

        return ret;
};

deque<string> segmentDNSName(const string& input )
{

#line 260 "dnslabeltext.cc"
static const char _dnsname_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 2, 0, 1, 
	2, 1, 5, 2, 4, 5, 3, 0, 
	1, 5
};

static const char _dnsname_key_offsets[] = {
	0, 0, 2, 4, 6, 8, 10, 12
};

static const unsigned char _dnsname_trans_keys[] = {
	46u, 92u, 46u, 92u, 48u, 57u, 48u, 57u, 
	48u, 57u, 46u, 92u, 46u, 92u, 0
};

static const char _dnsname_single_lengths[] = {
	0, 2, 2, 0, 0, 0, 2, 2
};

static const char _dnsname_range_lengths[] = {
	0, 0, 0, 1, 1, 1, 0, 0
};

static const char _dnsname_index_offsets[] = {
	0, 0, 3, 6, 8, 10, 12, 15
};

static const char _dnsname_trans_targs[] = {
	0, 3, 2, 7, 3, 2, 4, 2, 
	5, 0, 6, 0, 7, 3, 2, 0, 
	3, 2, 0
};

static const char _dnsname_trans_actions[] = {
	0, 3, 16, 0, 0, 11, 7, 5, 
	7, 0, 7, 0, 9, 9, 19, 0, 
	13, 22, 0
};

static const char _dnsname_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 1
};

static const int dnsname_start = 1;
static const int dnsname_first_final = 7;
static const int dnsname_error = 0;

static const int dnsname_en_main = 1;


#line 89 "dnslabeltext.rl"

	(void)dnsname_error;  // silence warnings
	(void)dnsname_en_main;
        const char *p = input.c_str(), *pe = input.c_str() + input.length();
        const char* eof = pe;
        int cs;
        char val = 0;

        string label;
        deque<string> ret;

        
#line 325 "dnslabeltext.cc"
	{
	cs = dnsname_start;
	}

#line 330 "dnslabeltext.cc"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _dnsname_trans_keys + _dnsname_key_offsets[cs];
	_trans = _dnsname_index_offsets[cs];

	_klen = _dnsname_single_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _dnsname_range_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	cs = _dnsname_trans_targs[_trans];

	if ( _dnsname_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _dnsname_actions + _dnsname_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 101 "dnslabeltext.rl"
	{ 
                        ret.push_back(label);
                        label.clear();
                }
	break;
	case 1:
#line 105 "dnslabeltext.rl"
	{ 
                        label.clear();
                }
	break;
	case 2:
#line 109 "dnslabeltext.rl"
	{
                  char c = *p;
                  label.append(1, c);
                }
	break;
	case 3:
#line 113 "dnslabeltext.rl"
	{
                  char c = *p;
                  val *= 10;
                  val += c-'0';
                  
                }
	break;
	case 4:
#line 119 "dnslabeltext.rl"
	{
                  label.append(1, val);
                  val=0;
                }
	break;
	case 5:
#line 124 "dnslabeltext.rl"
	{
                  label.append(1, *(p));
                }
	break;
#line 445 "dnslabeltext.cc"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _dnsname_actions + _dnsname_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 101 "dnslabeltext.rl"
	{ 
                        ret.push_back(label);
                        label.clear();
                }
	break;
#line 468 "dnslabeltext.cc"
		}
	}
	}

	_out: {}
	}

#line 137 "dnslabeltext.rl"


        if ( cs < dnsname_first_final ) {
                throw runtime_error("Unable to parse DNS name '"+input+"': cs="+std::to_string(cs));
        }

        return ret;
};


#if 0
int main()
{
	//char blah[]="\"blah\" \"bleh\" \"bloeh\\\"bleh\" \"\\97enzo\"";
  char blah[]="\"v=spf1 ip4:67.106.74.128/25 ip4:63.138.42.224/28 ip4:65.204.46.224/27 \\013\\010ip4:66.104.217.176/28 \\013\\010ip4:209.48.147.0/27 ~all\"";
  //char blah[]="\"abc \\097\\098 def\"";
  printf("Input: '%s'\n", blah);
	vector<string> res=dnstext(blah);
  cerr<<res.size()<<" segments"<<endl;
  cerr<<res[0]<<endl;
}
#endif
