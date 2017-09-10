#include <fstream>
#include <string.h>
#include <stdlib.h>
#include "head/test.h"

using namespace std;

#define max_blv_num 50
#define buf_size 8192
#define byte unsigned char
#define FLV_HEADER_LEN 0x9
#define PRE_TAG_SIZE_LEN 0x4
#define TAG_TYPE_LEN 0x1
#define DATA_SIZE_LEN 0x3
#define TIMESTAMP_LEN 0x4
#define STREAM_ID_LEN 0x3
#define TAG_HEADER_LEN 0xb
#define TAG_SCRIPT 0x12
#define TAG_AUDIO 0x8
#define TAG_VIDEO 0x9

long blv_dura[max_blv_num];
char path[100];

long parseLong(byte *bs, int digits);
void getTimeStamp(byte *bs);
void getTimeStampBack(byte* bs);
void writeMainBody(ifstream *is, ofstream *os);
void writeHeader(ifstream *is, ofstream *os);
void writeClippedBody(ifstream *is, ofstream *os, int seq_num);
void skipHeader(ifstream *is);
void skipScript(ifstream *is);
void getTSFromJSON();
long parseLongDec(char *c, int digits);
long pow(int x, int y);
string intTostring(int i);
byte *getNewTS(long ts);

int main() {
    ofstream os("Des Path", ios_base::binary|ios_base::trunc);  //Replace the Des Path with the Destination file path (e.g E:\\merge.flv)
    strcpy(path, "Src Path\\index.json");   //Repace "avid" with the video path,should be the parent folder of index.json
    getTSFromJSON();
    ifstream is("Src Path\\0.blv", ios_base::binary);
    writeHeader(&is, &os);
    skipScript(&is);
    writeMainBody(&is, &os);
    is.close();
    int i = 1;
    while(true)
    {
        string str = intTostring(i);
        string ppp = "Src Path\\" + str;
        cout << "writing " << ppp  << endl;
        is.open(ppp.c_str(), ios_base::binary);
        if(!is.is_open())
            break;
        skipHeader(&is);
        skipScript(&is);
        writeClippedBody(&is, &os, i - 1);
        is.close();
        ++i;
    }
    os.close();
    return 1;
}

string intTostring(int i)
{
    char c[6];
    int digits;
    if(i >= 100)
        digits = 3;
    else if(i >= 10 && i < 100)
        digits = 2;
    else
    {
        digits = 1;
    }
    c[digits] = '\0';
    while(i != 0)
    {
        c[digits - 1] = i%10 + '0';
        i = i/10;
        --digits;
    }
    string str(c);
    str.append(".blv");
    return str;
}

long parseLong(byte *bs, int digits)
{
	long res=0;
	for(int i=0;i<digits;i++)
		res += bs[i] << 8*(digits - i -1);
	return res;
}

void getTimeStamp(byte *bs)
{
	byte temp = bs[3];
	for(int i = 2; i >= 0; i--)
		bs[i+1] = bs[i];
	bs[0] = temp;
}

void getTimeStampBack(byte* bs)
{
    byte temp = bs[0];
	for(int i = 0; i < TIMESTAMP_LEN - 1; i++)
		bs[i] = bs[i + 1];
	bs[3] = temp;
}

void writeHeader(ifstream *is, ofstream *os)
{
    char header[FLV_HEADER_LEN + PRE_TAG_SIZE_LEN];
    is->read(header, sizeof(header));
    os->write(header, sizeof(header));
}

void writeMainBody(ifstream *is, ofstream *os)
{
    char buf[buf_size];
    while(!is->eof())
    {
        is->read(buf, sizeof(buf));
        int size_r;
        if((size_r = is->gcount()) < sizeof(buf))
        {
            os->write(buf, size_r);
            break;
        }
        else
        {
            os->write(buf, sizeof(buf));
        }
    }
}

void writeClippedBody(ifstream *is, ofstream *os, int seq_num)
{
    unsigned int tagType;
    while((tagType = is->get()) != EOF)
    {
        os->put(tagType);
        char ds[DATA_SIZE_LEN];
        is->read(ds, sizeof(ds));
        os->write(ds, sizeof(ds));
        long dataSize = parseLong((byte *)ds, sizeof(ds));
        char ts[TIMESTAMP_LEN];
        is->read(ts, sizeof(ts));
        getTimeStamp((byte *)ts);
        long curTS = parseLong((byte *)ts, TIMESTAMP_LEN);
        byte* newTS = getNewTS(curTS + blv_dura[seq_num]);
        os->write((char *)newTS, sizeof(newTS));
        char buf[buf_size];
        long written = 0;
        while(written + buf_size <= dataSize + STREAM_ID_LEN)
        {
            is->read(buf, sizeof(buf));
            os->write(buf, sizeof(buf));
            written += buf_size;
        }
        is->read(buf, dataSize + STREAM_ID_LEN - written);
        os->write(buf, dataSize +STREAM_ID_LEN - written);
        char pts[PRE_TAG_SIZE_LEN];
        is->read(pts, sizeof(pts));
        long preTagSize = parseLong((byte *)pts, sizeof(pts));
        if(preTagSize != dataSize + TAG_HEADER_LEN)
        {
            cout << "Read Error" << endl;
            exit(0);
        }
        os->write(pts, sizeof(pts));
    }
}

void skipHeader(ifstream *is)
{
    is->seekg(FLV_HEADER_LEN + PRE_TAG_SIZE_LEN, ios::cur);
}

void skipScript(ifstream *is)
{
    unsigned int tagType = is->get();
    if(tagType != TAG_SCRIPT)
        return;
    char ds[DATA_SIZE_LEN];
    is->read(ds, sizeof(ds));
    long dataSize = parseLong((byte *)ds, sizeof(ds));
    is->seekg(TIMESTAMP_LEN +
             STREAM_ID_LEN +
             dataSize,
             ios::cur);
    char pts[PRE_TAG_SIZE_LEN];
    is->read(pts, sizeof(pts));
    long preTagSize = parseLong((byte *)pts, sizeof(pts));
    if(preTagSize != dataSize + TAG_HEADER_LEN)
        exit(0);
}

void getTSFromJSON()
{
    ifstream is(path);
    int blv_num = 0;
    while(!is.eof())
    {
        string str;
        getline(is, str);
        size_t pos = 0;
        size_t idx;
        while((idx = str.find("\"duration\":", pos))
              != string::npos)
        {
            idx += 11;
            char number[10];
            int digits = 0;
            while(str[idx] != ',')
                if(str[idx] >= '0' && str[idx] <= '9')
                    number[digits++] = str[idx++];
            number[digits] = '\0';
            pos = idx;
            blv_dura[blv_num++] = parseLongDec(number, digits);
        }
    }
    for(int i = 1; i <blv_num; i++)
        blv_dura[i] += blv_dura[i - 1];
}

long parseLongDec(char *c, int digits)
{
    long res = 0;
    for(int i = 0; i < digits; i++)
    {
        int dd;
        res += dd = (c[i] - '0')*pow(10, digits - i - 1);
    }
    return res;
}

long pow(int x, int y)
{
    long res = x;
    if(y == 0)
        return 1;
    while(--y > 0)
        res *= x;
    return res;
}

byte *getNewTS(long ts)
{
    const int digits = 4;
	byte res[digits];
	for(int i = 0; i < digits; i++)
	{
		long t0 = ts >> 8*(digits - i - 1);
		long t1 = t0 << 8*(digits - 1);
		res[i] = t1 >> 8*(digits - 1);
	}
	getTimeStampBack(res);
	return res;
}





