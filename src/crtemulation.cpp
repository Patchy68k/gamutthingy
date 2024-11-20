#include "crtemulation.h"
#include "constants.h"
#include "matrix.h"
#include "vec3.h"

#include <stdio.h>
#include <math.h>
#include <numbers>
#include <cstring> //for memcpy

bool crtdescriptor::Initialize(double blacklevel, double whitelevel, int modulatorindex_in, int demodulatorindex_in, int renorm, bool doclamphigh, double clamplow, double clamphigh, int verbositylevel){
    bool output = true;
    verbosity = verbositylevel;
    CRT_EOTF_blacklevel = blacklevel;
    CRT_EOTF_whitelevel = whitelevel;
    Initialize1886EOTF();
    output = InitializeNTSC1953WhiteBalanceFactors();
    rgbclamplowlevel = clamplow;
    rgbclamphighlevel = clamphigh;
    clamphighrgb = doclamphigh;
    modulatorindex = modulatorindex_in;
    bool havemodulator = false;
    if (modulatorindex != CRT_MODULATOR_NONE){
        havemodulator = true;
        output = InitializeModulator();
    }
    demodulatorindex = demodulatorindex_in;
    demodulatorrenormalization = renorm;
    bool havedemodulator = false;
    if (demodulatorindex != CRT_DEMODULATOR_NONE){
        havedemodulator = true;
        InitializeDemodulator();
    }
    // get our modulator and demodulator into one matrix
    if (havemodulator || havedemodulator){
        if (havemodulator && havedemodulator){
            mult3x3Matrices(demodulatorMatrix, modulatorMatrix, overallMatrix);
            if (verbosity >= VERBOSITY_SLIGHT){
                printf("\n----------\nCombined modulator and demodulator matrix...\n");
                print3x3matrix(overallMatrix);
                printf("\n----------\n");
            }
        }
        else if (havemodulator){
            memcpy(overallMatrix, modulatorMatrix, 9 * sizeof(double));
        }
        else {
            memcpy(overallMatrix, demodulatorMatrix, 9 * sizeof(double));
        }
    }
    else {
        printf("Somehow initializing CRT emulation with no modulator or demodulator specified. This is bad and was supposed to be unreachable...\n");
        // use an identity matrix so we're not running uninitialized
        double identity[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
        memcpy(overallMatrix, identity, 9 * sizeof(double));
    }
    
    output = Invert3x3Matrix(overallMatrix,  inverseOverallMatrix);

    if (prpgpbscaling){
        InitializePrPgPbScaling();
    }

    return output;
}

void crtdescriptor::Initialize1886EOTF(){
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing CRT EOTF emulation...\n");
    }
    
    if (CRT_EOTF_blacklevel < 0.0){
        printf("Negative luminosity is impossible. Setting CRT black level to 0.\n");
        CRT_EOTF_blacklevel = 0.0;
    }
    if (CRT_EOTF_whitelevel < 0.0){
        printf("Negative luminosity is impossible. Setting CRT white level to 0.\n");
        CRT_EOTF_whitelevel = 0.0;
    }
    if (CRT_EOTF_blacklevel >= CRT_EOTF_whitelevel){
        printf("Black brighter than white is impossible. Setting black and white levels to defaults.\n");
        CRT_EOTF_blacklevel = 0.001;
        CRT_EOTF_whitelevel = 1.0;
    }
    BruteForce1886B();
    CRT_EOTF_k = CRT_EOTF_whitelevel / pow(1.0 + CRT_EOTF_b, 2.6);
    CRT_EOTF_s = pow(0.35 + CRT_EOTF_b, -0.4);
    CRT_EOTF_i = CRT_EOTF_k * pow(0.35 + CRT_EOTF_b, 2.6);

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("CRT emulation EOTF constants:\nblack level: %f\nwhite level: %f\nb: %.16f\nk: %.16f\ns: %.16f\ni: %.16f\n", CRT_EOTF_blacklevel, CRT_EOTF_whitelevel, CRT_EOTF_b, CRT_EOTF_k, CRT_EOTF_s, CRT_EOTF_i);
        
        //screen barf a bunch of test values to graph elsewhere and make sure it's right
        /*
        for (int i=0; i<=2000; i++){
            double testinput = i * 0.0005;
            double testoutput = tolinear1886appx1(testinput);
            printf("%f, %f\n", testinput, testoutput);
        }
        */
        /*
        printf("inverse EOTF tests:\n");
        double testinput = 0.0;
        double testoutput = tolinear1886appx1(testinput);
        double retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.25;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.5;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.75;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 1.0;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.35;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.3501;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = 0.3499;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        testinput = -0.1;
        testoutput = tolinear1886appx1(testinput);
        retestotput = togamma1886appx1(testoutput);
        printf("in %f, out %f, backwards %f\n", testinput, testoutput, retestotput);
        */
        
    }
    printf("\n----------\n\n");
    return;
}

// Brute force the value of "b" for the BT.1886 Appendix 1 EOTF function using binary search.
// blacklevel is CRT luminosity in cd/m^2 given black input, divided by 100 (sane value 0.001)
// whitelevel is CRT luminosity in cd/m^2 given white input, divided by 100 (sane value 1.0)
void crtdescriptor::BruteForce1886B(){
    
    if (CRT_EOTF_blacklevel == 0.0){
       CRT_EOTF_b = 0.0;
       return;
    }
    
    double floor = 0.0;
    double ceiling = 1.0;
    double guess;
    int iters = 0;
    
    while(true){
        guess = (floor + ceiling) * 0.5;
        double gresult = (CRT_EOTF_whitelevel / pow(1.0 + guess, 2.6)) * pow(0.35 + guess, -0.4) * pow(guess, 3.0);
        // exact hit!
        if (gresult == CRT_EOTF_blacklevel){
            break;
        }
        iters++;
        // should have converged by now
        if (iters > 100){
            break;
        }
        double error = fabs(gresult - CRT_EOTF_blacklevel);
        // wolfram alpha was giving 16 digits, so lets hope for the same
        if (error < 1e-16){
            break;
        }
        if (gresult > CRT_EOTF_blacklevel){
            ceiling = guess;
        }
        else {
            floor = guess;
        }
    }
    
    CRT_EOTF_b = guess;
    return;
}

// The EOTF function from BT.1886 Appendix 1 for approximating the behavior of CRT televisions.
// The function from Appendix 1 is more faithful than the fairly useless Annex 1 function, which is just 2.4 gamma
// The function has been modified to handle negative inputs in the same fashion as IEC 61966-2-4 (a.k.a. xvYCC)
// (BT.1361 does something similar.)
// Dynamic range is restored in a post-processing step that chops off the black lift and then normalizes to 0-1.
// Initialize1886EOTF() must be run once before this can be used.
double crtdescriptor::tolinear1886appx1(double input){
    
    // Shift input by b
    input += CRT_EOTF_b;
    
    // Handle negative input by flipping sign 
    bool flipsign = false;
    if (input < 0){
        input *= -1.0;
        flipsign = true;
    }
    
    // The main EOTF function
    double output;
    if (input < (0.35 + CRT_EOTF_b)){
        output = CRT_EOTF_k * CRT_EOTF_s * pow(input, 3.0);
    }
    else {
        output = CRT_EOTF_k * pow(input, 2.6);
    }
    
    // Flip sign again if input was negative
    if (flipsign){
        output *= -1.0;
    }
    
    // Chop off the black lift and normalize to 0-1
    output -= CRT_EOTF_blacklevel;
    output /= (CRT_EOTF_whitelevel - CRT_EOTF_blacklevel);
    
    // fix floating point errors very near 0 or 1
    if ((output != 0.0) && (fabs(output - 0.0) < 1e-6)){
        output = 0.0;
    }
    else if ((output != 1.0) && (fabs(output - 1.0) < 1e-6)){
        output = 1.0;
    }
    
    return output;
}

// inverse of tolinear1886appx1()
// Initialize1886EOTF() must be run once before this can be used.
double crtdescriptor::togamma1886appx1(double input){
    
    // undo the chop and normalization post-processing
    input *= (CRT_EOTF_whitelevel - CRT_EOTF_blacklevel);
    input += CRT_EOTF_blacklevel;
    
    // Handle negative input by flipping sign 
    bool flipsign = false;
    if (input < 0){
        input *= -1.0;
        flipsign = true;
    }
    
    // The main EOTF function
    double output;
    if (input < CRT_EOTF_i){
        output = pow((1.0/CRT_EOTF_k) * (1.0/CRT_EOTF_s) * input, 1.0/3.0);
    }
    else {
        output = pow((1.0/CRT_EOTF_k) * input, 1.0/2.6);
    }
    
    // Flip sign again if input was negative
    if (flipsign){
        output *= -1.0;
    }
    
    //unshift
    output -= CRT_EOTF_b;
    
    // fix floating point errors very near 0 or 1
    if ((output != 0.0) && (fabs(output - 0.0) < 1e-6)){
        output = 0.0;
    }
    else if ((output != 1.0) && (fabs(output - 1.0) < 1e-6)){
        output = 1.0;
    }
    
    return output;
    
}

// set the global variables for NTSC 1953 white balance
// This is basically copy/paste from the first few steps of gamut boundary initialization.
bool crtdescriptor::InitializeNTSC1953WhiteBalanceFactors(){
    
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing NTSC 1953 white balance factors for CRT de/modulator emulation...\n");
    }
    
    bool output = true;
    
    const double matrixP[3][3] = {
        {gamutpoints[GAMUT_NTSC][0][0], gamutpoints[GAMUT_NTSC][1][0], gamutpoints[GAMUT_NTSC][2][0]},
        {gamutpoints[GAMUT_NTSC][0][1], gamutpoints[GAMUT_NTSC][1][1], gamutpoints[GAMUT_NTSC][2][1]},
        {gamutpoints[GAMUT_NTSC][0][2], gamutpoints[GAMUT_NTSC][1][2], gamutpoints[GAMUT_NTSC][2][2]}
    };
    
    double inverseMatrixP[3][3];
    output = Invert3x3Matrix(matrixP, inverseMatrixP);
    if (!output){
        printf("Disaster! Matrix P is not invertible! Bad stuff will happen!\n");       
    }
    
    vec3 matrixW;
    matrixW.x = whitepoints[WHITEPOINT_ILLUMINANTC][0] / whitepoints[WHITEPOINT_ILLUMINANTC][1];
    matrixW.y = 1.0;
    matrixW.z = whitepoints[WHITEPOINT_ILLUMINANTC][2] / whitepoints[WHITEPOINT_ILLUMINANTC][1];
    
    vec3 normalizationFactors = multMatrixByColor(inverseMatrixP, matrixW);
    
    double matrixC[3][3] = {
        {normalizationFactors.x, 0.0, 0.0},
        {0.0, normalizationFactors.y, 0.0},
        {0.0, 0.0, normalizationFactors.z}
    };
    
    double matrixNPM[3][3];
    mult3x3Matrices(matrixP, matrixC, matrixNPM);
    
    ntsc1953_wr = matrixNPM[1][0];
    ntsc1953_wg = matrixNPM[1][1];
    ntsc1953_wb = matrixNPM[1][2];
    
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("wr: %.10f, wg: %.10f, wb: %.10f\n----------\n", ntsc1953_wr, ntsc1953_wg, ntsc1953_wb);
    }

    ntsc1953maxpr = ntsc1953_wg + ntsc1953_wb;
    ntsc1953maxpg = ntsc1953_wr + ntsc1953_wb;
    ntsc1953maxpb = ntsc1953_wr + ntsc1953_wg;
    
    return output;
}

void crtdescriptor::InitializeDemodulator(){
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing CRT demodulator matrix...\n");
    }
    
    // convert angles to radians
    double redangle = demodulatorinfo[demodulatorindex][0][0] * (std::numbers::pi_v<long double>/ 180.0); 
    double greenangle = demodulatorinfo[demodulatorindex][0][1] * (std::numbers::pi_v<long double>/ 180.0); 
    double blueangle = demodulatorinfo[demodulatorindex][0][2] * (std::numbers::pi_v<long double>/ 180.0); 
    
    // gains
    double redgain = demodulatorinfo[demodulatorindex][1][0];
    double greengain = demodulatorinfo[demodulatorindex][1][1];
    double bluegain = demodulatorinfo[demodulatorindex][1][2];
    // gains are probably normalized to blue, probably to 1.0.
    // if blue is near 2.03, then it's not normalized. Let's guess 1.8 for a good cutoff.
    if (bluegain < 1.8){
        if (bluegain != 1.0){
            printf("B-Y gain is %f rather than 1.0. Nevertheless assuming gains are normalized to blue.\n", bluegain);
        }
        double normfactor = Uupscale;

         // Theoretically, we should be adjusting the adjusting the normalization factor when blue's angle is non-zero or gain is non-one
        // However, the CXA1213AS...
            // has red/green gains shown in the datasheets are closer to other Sony chips if NOT scaled,
            // and the resulting matrix is closer to other Sony chips if NOT scaled.
        // On the other hand, TDA8362 looks wildly wrong if not renormalized
        // So punt the decision to the user...
        bool dorenorm = false;
        bool weirdgain = (bluegain != 1.0);
        bool weirdangle = (blueangle != 0.0);
        switch (demodulatorrenormalization){
            case (RENORM_DEMOD_NONE):
                dorenorm = false;
                break;
            case (RENORM_DEMOD_INSANE):
                dorenorm = (weirdgain && weirdangle);
                break;
            case (RENORM_DEMOD_ANGLE_NOT_ZERO):
                dorenorm = weirdangle;
                break;
            case (RENORM_DEMOD_GAIN_NOT_ONE):
                dorenorm = weirdgain;
                break;
            case (RENORM_DEMOD_ANY):
                dorenorm = (weirdgain || weirdangle);
                break;
            default:
                break;
        };
        if (dorenorm){
            // Y'UV upscale factors form an ellipse with Uupscale on one axis and Vupscale on the other
            // radius of ellipse at theta is (a*b)/sqrt((a*sin(theta))^2 + (b*cos(theta))^2)
            double Aupscale = Uupscale;
            if (weirdangle){
                Aupscale = (Uupscale * Vupscale) / sqrt((Uupscale * Uupscale * sin(blueangle) * sin(blueangle)) + (Vupscale * Vupscale * cos(blueangle) * cos(blueangle)));
            }
            // compute factor such that denormalized blue gain is the upscale factor at that angle
            normfactor = Aupscale / bluegain;
        }



        redgain *= normfactor;
        greengain *= normfactor;
        bluegain *= normfactor;
    }
    else {
        printf("B-Y gain is %f. Assuming gains are not normalized.\n", bluegain);
    }
    
    //printf("angles: red %f, green %f, blue %f\ngains: red %f, green %f, blue %f\n", redangle, greenangle, blueangle, redgain, greengain, bluegain);
    
    // depolarize to get UV coordinates
    double redx = redgain * cos(redangle);
    double redy = redgain * sin(redangle);
    double greenx = greengain * cos(greenangle);
    double greeny = greengain * sin(greenangle);
    double bluex = bluegain * cos(blueangle);
    double bluey = bluegain * sin(blueangle);
    
    //printf("UV coords: red: x %f, y %f; green x %f, y %f; blue x %f y %f\n", redx, redy, greenx, greeny, bluex, bluey);
    
    // constant matrix
    const double matrixB[2][2] = {
        {(1.0 - ntsc1953_wr) / Vupscale, (-1.0 * ntsc1953_wr) / Uupscale},
        {(-1.0 * ntsc1953_wg) / Vupscale, (-1.0 * ntsc1953_wg) / Uupscale}
    };
    
    //printf("MatrixB: [[ %f, %f ], [ %f, %f ]]\n", matrixB[0][0], matrixB[0][1], matrixB[1][0], matrixB[1][1]);
    
    // multiply matrixB by {{y},{x}} and add  {{wr},{wg}} to recover Krr, Krg, Kgr, Kgg, Kbr, and Kbg
    // (multiply 2x2 matrix by 1x2 matrix, maybe implement a function for this if we wind up needing to do it elsewhere)
    double Krr = (matrixB[0][0] * redy) + (matrixB[0][1] * redx) + ntsc1953_wr;
    double Krg = (matrixB[1][0] * redy) + (matrixB[1][1] * redx) + ntsc1953_wg;
    double Kgr = (matrixB[0][0] * greeny) + (matrixB[0][1] * greenx) + ntsc1953_wr;
    double Kgg = (matrixB[1][0] * greeny) + (matrixB[1][1] * greenx) + ntsc1953_wg;
    double Kbr = (matrixB[0][0] * bluey) + (matrixB[0][1] * bluex) + ntsc1953_wr;
    double Kbg = (matrixB[1][0] * bluey) + (matrixB[1][1] * bluex) + ntsc1953_wg;
    // recover Krb, Kgb, and Kbb by virtue of matrixK's rows being normalized to 1.0
    double Krb = 1.0 - (Krr + Krg);
    double Kgb = 1.0 - (Kgr + Kgg);
    double Kbb = 1.0 - (Kbr + Kbg);

    // copy our correction matrix
    // (or the identity matrix if dummy demodulator)
    if (demodulatorindex == CRT_DEMODULATOR_DUMMY){
        demodulatorMatrix[0][0] = 1.0;
        demodulatorMatrix[0][1] = 0.0;
        demodulatorMatrix[0][2] = 0.0;
        demodulatorMatrix[1][0] = 0.0;
        demodulatorMatrix[1][1] = 1.0;
        demodulatorMatrix[1][2] = 0.0;
        demodulatorMatrix[2][0] = 0.0;
        demodulatorMatrix[2][1] = 0.0;
        demodulatorMatrix[2][2] = 1.0;
    }
    else {
        demodulatorMatrix[0][0] = Krr;
        demodulatorMatrix[0][1] = Krg;
        demodulatorMatrix[0][2] = Krb;
        demodulatorMatrix[1][0] = Kgr;
        demodulatorMatrix[1][1] = Kgg;
        demodulatorMatrix[1][2] = Kgb;
        demodulatorMatrix[2][0] = Kbr;
        demodulatorMatrix[2][1] = Kbg;
        demodulatorMatrix[2][2] = Kbb;
    }
    
    // screen barf
    if (verbosity >= VERBOSITY_SLIGHT){
        print3x3matrix(demodulatorMatrix);
        printf("\n----------\n");
    }
    
    return;
}


bool crtdescriptor::InitializeModulator(){
    
    bool output = true;
    
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing CRT modulator matrix...\n");
    }
    
    // convert angles to radians
    double redangle = modulatorinfo[modulatorindex][0][0] * (std::numbers::pi_v<long double>/ 180.0); 
    double greenangle = modulatorinfo[modulatorindex][0][1] * (std::numbers::pi_v<long double>/ 180.0); 
    double blueangle = modulatorinfo[modulatorindex][0][2] * (std::numbers::pi_v<long double>/ 180.0); 
    
    //printf("angles: red %f, green %f, blue %f\n", redangle, greenangle, blueangle);
    
    // get voltages
    double burstvpp = modulatorinfo[modulatorindex][2][0];
    double maxwhitev = modulatorinfo[modulatorindex][2][1];
    // White voltage corresponds to 0-1 scale for Y'
    // maxwhitev is supposed to be 100 IRE (5/7v ~= 0.71v)
    // Burst is a sin wave centered at 0 IRE with its peak at 1/2 burstvpp.
    // Burst's peak is supposed to be 20 IRE (1/7v ~=0.14v)
    // If things are to spec, we multiply the color/burst ratio values by 0.2 (burst peak / maxwhitev) to get them on the same 0-1 scale as Y'.
    // Of course, things may not be to spec, so let's calculate that multiplier.
    double burstpeakoverwhite = burstvpp / (2.0 * maxwhitev);
    
    //printf("maxwhitev %f, burstvpp %f, ratio %f\n", maxwhitev, burstvpp, burstpeakoverwhite);
    
    // color multipliers
    double redmult = modulatorinfo[modulatorindex][1][0] * burstpeakoverwhite;
    double greenmult = modulatorinfo[modulatorindex][1][1] * burstpeakoverwhite;
    double bluemult = modulatorinfo[modulatorindex][1][2] * burstpeakoverwhite;
    
    //printf("multipliers: red %f, green %f, blue %f\n", redmult, greenmult, bluemult);
    
    // create R'G'B' to Y'UV matrix
    double RGBtoYUV[3][3] = {
        {ntsc1953_wr, ntsc1953_wg, ntsc1953_wb}, // use the high precision NTSC 1953 white balance
        {redmult * cos(redangle), greenmult * cos(greenangle), bluemult * cos(blueangle)},
        {redmult * sin(redangle), greenmult * sin(greenangle), bluemult * sin(blueangle)}
    };
    //printf("R'G'B' to Y'UV:\n");
    //print3x3matrix(RGBtoYUV);
    
    
    // in order to get back to R'G'B', we need an idealized Y'UV to R'G'B' matrix
    
    // idealized R'G'B' to Y'PbPr (a.k.a. Y'(B'-Y')(R'-Y'))
    double idealRGBtoYPbPr[3][3] = {
        {ntsc1953_wr, ntsc1953_wg, ntsc1953_wb}, // use the high precision NTSC 1953 white balance
        {-1.0 * ntsc1953_wr, -1.0 * ntsc1953_wg, ntsc1953_wr + ntsc1953_wg},
        {ntsc1953_wg + ntsc1953_wb, -1.0 * ntsc1953_wg, -1.0 * ntsc1953_wb}
    };
    
    // idealized Y'PbPr to Y'UV
    double idealYPbPrtoYUV[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, Udownscale, 0.0},
        {0.0, 0.0, Vdownscale}
    };
    
    // idealized R'G'B' to Y'UV
    double idealRGBtoYUV[3][3];
    mult3x3Matrices(idealYPbPrtoYUV, idealRGBtoYPbPr, idealRGBtoYUV);
    
    // invert to get idealized Y'UV to R'G'B'
    double idealYUVtoRGB[3][3];
    output = Invert3x3Matrix(idealRGBtoYUV, idealYUVtoRGB);
    if (!output){
        printf("Disaster! Matrix idealRGBtoYUV is not invertible! Bad stuff will happen!\n");       
    }
    //printf("Ideal Y'UV to R'G'B':\n");
    //print3x3matrix(idealYUVtoRGB);
    
    // multiply  the idealized Y'UV to R'G'B' matrix with the modulator's R'B'G' to Y'UV matrix to get a R'G'B' to R'G'B' matrix
    double RGBtoRGB[3][3];
    mult3x3Matrices(idealYUVtoRGB, RGBtoYUV, RGBtoRGB);
    
    // normalize to compensate rounding errors due to (typically) only two decimal places in the datasheet
    // and copy to crtdescriptor object
    double row0sum = RGBtoRGB[0][0] + RGBtoRGB[0][1] + RGBtoRGB[0][2];
    double row1sum = RGBtoRGB[1][0] + RGBtoRGB[1][1] + RGBtoRGB[1][2];
    double row2sum = RGBtoRGB[2][0] + RGBtoRGB[2][1] + RGBtoRGB[2][2];
    modulatorMatrix[0][0] = RGBtoRGB[0][0] / row0sum;
    modulatorMatrix[0][1] = RGBtoRGB[0][1] / row0sum;
    modulatorMatrix[0][2] = RGBtoRGB[0][2] / row0sum;
    modulatorMatrix[1][0] = RGBtoRGB[1][0] / row1sum;
    modulatorMatrix[1][1] = RGBtoRGB[1][1] / row1sum;
    modulatorMatrix[1][2] = RGBtoRGB[1][2] / row1sum;
    modulatorMatrix[2][0] = RGBtoRGB[2][0] / row2sum;
    modulatorMatrix[2][1] = RGBtoRGB[2][1] / row2sum;
    modulatorMatrix[2][2] = RGBtoRGB[2][2] / row2sum;
    
    // screen barf
    if (verbosity >= VERBOSITY_SLIGHT){
        print3x3matrix(modulatorMatrix);
        printf("\n----------\n");
    }
    
    return output;
    
}

void crtdescriptor::InitializePrPgPbScaling(){
    adjntsc1953maxpr = ntsc1953maxpr * prpgpbscalemax;
    adjntsc1953maxpg = ntsc1953maxpg * prpgpbscalemax;
    adjntsc1953maxpb = ntsc1953maxpb * prpgpbscalemax;

    prelbow = ntsc1953maxpr * prpgpbscaleelbow;
    pgelbow = ntsc1953maxpg * prpgpbscaleelbow;
    pbelbow = ntsc1953maxpb * prpgpbscaleelbow;

    prhand = adjntsc1953maxpr - prelbow;
    pghand = adjntsc1953maxpg - pgelbow;
    pbhand = adjntsc1953maxpb - pbelbow;

    vec3 color = multMatrixByColor(overallMatrix, vec3(1.0, 0.0, 0.0));
    double luma = (color.x * ntsc1953_wr) + (color.y * ntsc1953_wg) + (color.z * ntsc1953_wb);
    maxpr = color.x - luma;
    prneedsscale = (maxpr > adjntsc1953maxpr);

    color = multMatrixByColor(overallMatrix, vec3(0.0, 1.0, 0.0));
    luma = (color.x * ntsc1953_wr) + (color.y * ntsc1953_wg) + (color.z * ntsc1953_wb);
    maxpg = color.y - luma;
    pgneedsscale = (maxpg > adjntsc1953maxpg);

    color = multMatrixByColor(overallMatrix, vec3(0.0, 0.0, 1.0));
    luma = (color.x * ntsc1953_wr) + (color.y * ntsc1953_wg) + (color.z * ntsc1953_wb);
    maxpb = color.z - luma;
    pbneedsscale = (maxpb > adjntsc1953maxpb);

    printf("PrPgPb scaling %i, %i, %i\n", prneedsscale, pgneedsscale, pbneedsscale);

    return;
}

vec3 crtdescriptor::ScalePrPgPb(vec3 input){

    vec3 output = input;

    double luma = (input.x * ntsc1953_wr) + (input.y * ntsc1953_wg) + (input.z * ntsc1953_wb);
    double pr = input.x - luma;
    double pg = input.y - luma;
    double pb = input.z - luma;

    if (prneedsscale && (pr > prelbow)){
        pr = prelbow + (prhand * ((pr - prelbow) / (maxpr - prelbow)));
        output.x = pr + luma;
    }

    if (pgneedsscale && (pg > pgelbow)){
        pg = pgelbow + (pghand * ((pg - pgelbow) / (maxpg - pgelbow)));
        output.y = pg + luma;
    }

    if (pbneedsscale && (pb > pbelbow)){
        pb = pbelbow + (pbhand * ((pb - pbelbow) / (maxpb - pbelbow)));
        output.z = pb + luma;
    }
    return output;
}

bool crtdescriptor::PrPgPrBpBoundsCheck(vec3 input){
    double luma = (input.x * ntsc1953_wr) + (input.y * ntsc1953_wg) + (input.z * ntsc1953_wb);
    double pr = input.x - luma;
    double pg = input.y - luma;
    double pb = input.z - luma;
    if (pr > adjntsc1953maxpr){return false;}
    if (pg > adjntsc1953maxpg){return false;}
    if (pb > adjntsc1953maxpb){return false;}
    return true;
}

vec3 crtdescriptor::tolinear1886appx1vec3(vec3 input){
    vec3 output;
    output.x = tolinear1886appx1(input.x);
    output.y = tolinear1886appx1(input.y);
    output.z = tolinear1886appx1(input.z);
    return output;
}

vec3 crtdescriptor::togamma1886appx1vec3(vec3 input){
    vec3 output;
    output.x = togamma1886appx1(input.x);
    output.y = togamma1886appx1(input.y);
    output.z = togamma1886appx1(input.z);
    return output;
}

vec3 crtdescriptor::CRTEmulateGammaSpaceRGBtoLinearRGB(vec3 input){
    vec3 output = multMatrixByColor(overallMatrix, input);

    if (prpgpbscaling){
        output = ScalePrPgPb(output);
    }

    // clamp rgb
    // Real CRT probably did this, though who knows what the threshholds were.
    // We can sort of guess that maybe clipping here roughly mirrored clipping guidelines for broadcast media.
    // Broadcast signals absolutely must be clipped to -20 to 120 IRE in order to be modulated onto the carrier.
    // Some internet comments suggest clipping broadcast signals to 104 or 110 IRE "super white" was common.
    // Grading programs like DaVinci Resolve offer clipping modes of 0 to 100, -10 to 110, and -20 to 120.
    // The gap between reference black and blanking level on US NTSC was 7.5/92.5 (about -0.081). Going below that caused trouble on some CRTs.
    // We absolutely need to clamp low values.
    // Even if the CRT jungle chip isn't actively clamping, at some point there are just zero volts driving the electron gun.
    // Additionally, the Jzazbz PQ function is going to return NAN given inputs that are too negative.
    // (These colors are forced to black to prevent a segfault)
    // According to a couple test runs, we can get away with -0.1, but -0.2 borks the PQ function.
    // With this clamp in place, the only problematic input observed so far is the NES's 0xD "super black"
    // (Which *should* be forced to black, so that's OK.)
    // We don't *need* to clamp high values. The gamut compression algorithm will compress them.
    // Clamping will distort these colors. Though, that distortion is probably truer to what CRTs actually did.
    if (clamphighrgb){
        if (output.x > rgbclamphighlevel){output.x = rgbclamphighlevel;}
        if (output.y > rgbclamphighlevel){output.y = rgbclamphighlevel;}
        if (output.z > rgbclamphighlevel){output.z = rgbclamphighlevel;}
    }
    if (output.x < rgbclamplowlevel){output.x = rgbclamplowlevel;}
    if (output.y < rgbclamplowlevel){output.y = rgbclamplowlevel;}
    if (output.z < rgbclamplowlevel){output.z = rgbclamplowlevel;}

    output.x = tolinear1886appx1(output.x);
    output.y = tolinear1886appx1(output.y);
    output.z = tolinear1886appx1(output.z);
    return output;
}

vec3 crtdescriptor::CRTEmulateLinearRGBtoGammaSpaceRGB(vec3 input){
    input.x = togamma1886appx1(input.x);
    input.y = togamma1886appx1(input.y);
    input.z = togamma1886appx1(input.z);
    vec3 output = multMatrixByColor(inverseOverallMatrix, input);
    return output;
}
