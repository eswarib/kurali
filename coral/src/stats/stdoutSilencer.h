#ifndef _stdoutSilencer_h_
#define _stdoutSilencer_h_

class stdoutSilencer
{
public:
    stdoutSilencer();
    ~stdoutSilencer();

private:
    int originalStdout;
    stdoutSilencer(stdoutSilencer&);
    stdoutSilencer operator=(stdoutSilencer&);
};

#endif
