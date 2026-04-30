#ifndef _stderrSilencer_h_
#define _stderrSilencer_h_

class stderrSilencer
{
public:
    stderrSilencer();
    ~stderrSilencer();

private:
    int originalStderr;
    stderrSilencer(stderrSilencer&);
    stderrSilencer operator=(stderrSilencer&);
};

#endif
