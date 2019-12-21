
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/strsup.h>
#include <rdlib/Thread.h>
#include <rdlib/ThreadLock.h>
#include <rdlib/QuitHandler.h>

typedef std::vector<uint8_t> SIGDATA;

class Process : public Thread
{
public:
    Process() : nthread(nthreads++) {}
    virtual ~Process() {}

    static void SetParameters(uint_t _dim, const SIGDATA& _data1, const SIGDATA& _data2) {
        ThreadLock lock(globallock);
        dim   = _dim;
        data1 = &_data1;
        data2 = &_data2;
    }

protected:
    virtual void *Run() {
        const uint8_t * const ptr1 = &(*data1)[0];
        const uint8_t * const ptr2 = &(*data2)[0];
        const uint_t framesize = dim * dim * 3;
        const sint_t nframes1 = (sint_t)(data1->size() / framesize);
        const sint_t nframes2 = (sint_t)(data2->size() / framesize);
        const sint_t range = (sint_t)(fps * dist);
        const uint_t total = (2 * range) / step1;
        sint_t i, j;
        sint_t offset;
        uint_t k, pc = 0;

        {
            ThreadLock lock(globallock);
            fprintf(stderr, "Starting thread %u of %u\n", nthread, nthreads);
        }

        for (offset = -range + (sint_t)(nthread * step1); (offset < range) && !HasQuit(); offset += step1 * nthreads) {
            std::vector<sint_t> buffer((uint_t)fps);
            sint64_t sum = 0;

            for (i = 0; (i < nframes1); i += step2) {
                sint_t val = 0;

                j = i + offset;

                if (RANGE(j, 0, nframes2 - 1)) {
                    const uint_t p1 = (uint_t)i * framesize;
                    const uint_t p2 = (uint_t)j * framesize;

                    for (k = 0; (k < framesize); k++) {
                        sint_t d = (sint_t)ptr1[p1 + k] - (sint_t)ptr2[p2 + k];
                        val -= abs(d);
                    }
                }

                uint_t p = (uint_t)i % buffer.size();
                sum -= buffer[p];
                buffer[p] = val;
                sum += buffer[p];

                if (!((uint_t)i % 5) && RANGE(j, 0, nframes2 - 1)) {
                    double v = (double)sum / (double)(framesize * buffer.size()) + threshold;
                    if (v >= 0.0) {
                        ThreadLock lock(globallock);
                        printf("%0.2lf %0.2lf %0.2lf %0.14le\n",
                               (double)i / fps,
                               (double)j / fps,
                               (double)(i - j) / fps,
                               v);
                        fflush(stdout);
                    }
                }
            }

            {
                ThreadLock lock(globallock);
                completed++;

                uint_t _pc = (100 * completed) / total;
                if (_pc > pc) {
                    pc = _pc;
                    fprintf(stderr, "\rProcessing %u%%", pc);
                    fflush(stderr);
                }
            }
        }

        return NULL;
    }

protected:
    uint_t nthread;

    static ThreadLockObject globallock;
    static uint_t nthreads;

    static const SIGDATA *data1;
    static const SIGDATA *data2;
    static uint_t dim;
    static uint_t step1;
    static uint_t step2;
    static double fps;
    static double dist;
    static double threshold;

    static uint_t completed;
};

ThreadLockObject Process::globallock;
uint_t Process::nthreads  = 0;

const SIGDATA *Process::data1 = NULL;
const SIGDATA *Process::data2 = NULL;
uint_t Process::dim       = 0;
uint_t Process::step1     = 1;
uint_t Process::step2     = 5;

double Process::fps       = 25.0;
double Process::dist      = 10.0 * 60.0;
double Process::threshold = 1.0;

uint_t Process::completed = 0;

int main(int argc, char *argv[])
{
    AQuitHandler quithandler;

    if (argc < 3) {
        fprintf(stderr, "Usage: sigcmp [-n <n>] <file1> <file2>\n");
        exit(1);
    }

    uint_t n = 5;
    int arg = 1;

    if (stricmp(argv[arg], "-n") == 0) {
        n = (uint_t)AString(argv[++arg]);
        arg++;
    }

    SIGDATA fdata1;
    SIGDATA fdata2;

    AStdFile f;
    if (f.open(argv[arg], "rb")) {
        f.seek(0, SEEK_END);
        fdata1.resize(f.tell());
        f.rewind();

        if (f.readbytes(&fdata1[0], fdata1.size()) != (slong_t)fdata1.size()) {
            fprintf(stderr, "Failed to read %lu bytes from '%s'\n", (ulong_t)fdata1.size(), argv[arg]);
            fdata1.resize(0);
        }
        f.close();
    }
    else fprintf(stderr, "Failed to open file '%s' for reading\n", argv[arg]);

    arg++;

    if (fdata1.size()) {
        if (f.open(argv[arg], "rb")) {
            f.seek(0, SEEK_END);
            fdata2.resize(f.tell());
            f.rewind();

            if (f.readbytes(&fdata2[0], fdata2.size()) != (slong_t)fdata2.size()) {
                fprintf(stderr, "Failed to read %lu bytes from '%s'\n", (ulong_t)fdata2.size(), argv[arg]);
                fdata2.resize(0);
            }
            f.close();
        }
        else fprintf(stderr, "Failed to open file '%s' for reading\n", argv[arg]);
    }

    arg++;

    if (fdata1.size() && fdata2.size()) {
        Process processors[5];
        uint_t i;

        Process::SetParameters(n, fdata1, fdata2);

        for (i = 0; i < NUMBEROF(processors); i++) {
            processors[i].Start();
        }

        for (i = 0; i < NUMBEROF(processors); i++) {
            processors[i].Stop();
        }

        fprintf(stderr, "\rProcessing done  \n");
        fflush(stderr);
    }

    return 0;
}
