#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstddef>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "config.h"

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include <map>
#include <sstream>
#include <climits>
#include <set>
using namespace std;

#include "instConfig.h"
// DyninstAPI includes
#include "BPatch.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_flowGraph.h"
#include "BPatch_function.h"
#include "BPatch_point.h"


using namespace Dyninst;

//hash table length

// static u32 num_conditional, // the number of total conditional edges
//             num_indirect,   // the number of total indirect edges
//             max_map_size, // the number of all edges, including potential indirect edges
static u32 AllEdge_id = 0; // assign unique id for each conditional edges


//cmd line options
char *originalBinary;
char *instrumentedBinary;
bool verbose = false;
// set<string> instrumentLibraries;
// set<string> runtimeLibraries;



// call back functions
BPatch_function *ConditionJump;
BPatch_function *IndirectEdges;
BPatch_function *initAflForkServer;



const char *instLibrary = "./libCollAFLDyninst.so";

static const char *OPT_STR = "i:o:v";
static const char *USAGE = " -i <binary> -o <binary>\n \
    Analyse options:\n \
            -i: Input binary \n \
            -o: Output binary\n \
            -v: Verbose output\n";

bool parseOptions(int argc, char **argv)
{

    int c;
    while ((c = getopt (argc, argv, OPT_STR)) != -1) {
        switch ((char) c) {
        case 'i':
            originalBinary = optarg;
            break;
        case 'o':
            instrumentedBinary = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        default:
            cerr << "Usage: " << argv[0] << USAGE;
            return false;
        }
    }

    if(originalBinary == NULL) {
        cerr << "Input binary is required!"<< endl;
        cerr << "Usage: " << argv[0] << USAGE;
        return false;
    }

    if(instrumentedBinary == NULL) {
        cerr << "Output binary is required!" << endl;
        cerr << "Usage: " << argv[0] << USAGE;
        return false;
    }

    return true;
}

BPatch_function *findFuncByName (BPatch_image * appImage, char *funcName)
{
    BPatch_Vector < BPatch_function * >funcs;

    if (NULL == appImage->findFunction (funcName, funcs) || !funcs.size ()
        || NULL == funcs[0]) {
        cerr << "Failed to find " << funcName << " function." << endl;
        return NULL;
    }

    return funcs[0];
}

//skip some functions
bool isSkipFuncs(char* funcName){
    if (string(funcName) == string("first_init") ||
        string(funcName) == string("__mach_init") ||
        string(funcName) == string("_hurd_init") ||
        string(funcName) == string("_hurd_preinit_hook") ||
        string(funcName) == string("doinit") ||
        string(funcName) == string("doinit1") ||
        string(funcName) == string("init") ||
        string(funcName) == string("init1") ||
        string(funcName) == string("_hurd_subinit") ||
        string(funcName) == string("init_dtable") ||
        string(funcName) == string("_start1") ||
        string(funcName) == string("preinit_array_start") ||
        string(funcName) == string("_init") ||
        string(funcName) == string("init") ||
        string(funcName) == string("fini") ||
        string(funcName) == string("_fini") ||
        string(funcName) == string("_hurd_stack_setup") ||
        string(funcName) == string("_hurd_startup") ||
        string(funcName) == string("register_tm_clones") ||
        string(funcName) == string("deregister_tm_clones") ||
        string(funcName) == string("frame_dummy") ||
        string(funcName) == string("__do_global_ctors_aux") ||
        string(funcName) == string("__do_global_dtors_aux") ||
        string(funcName) == string("__libc_csu_init") ||
        string(funcName) == string("__libc_csu_fini") ||
        string(funcName) == string("start") ||
        string(funcName) == string("_start") || 
        string(funcName) == string("__libc_start_main") ||
        string(funcName) == string("__gmon_start__") ||
        string(funcName) == string("__cxa_atexit") ||
        string(funcName) == string("__cxa_finalize") ||
        string(funcName) == string("__assert_fail") ||
        string(funcName) == string("_dl_start") || 
        string(funcName) == string("_dl_start_final") ||
        string(funcName) == string("_dl_sysdep_start") ||
        string(funcName) == string("dl_main") ||
        string(funcName) == string("_dl_allocate_tls_init") ||
        string(funcName) == string("_dl_start_user") ||
        string(funcName) == string("_dl_init_first") ||
        string(funcName) == string("_dl_init")) {
        return true; //skip these functions
        }
    return false;    
}



// instrument at conditional edges, like afl
bool instrumentCondition(BPatch_binaryEdit * appBin, BPatch_function * instFunc, BPatch_point * instrumentPoint, 
         Dyninst::Address block_addr, u32 cond_id){
    vector<BPatch_snippet *> cond_args;
    BPatch_constExpr CondID(cond_id);
    cond_args.push_back(&CondID);

    BPatch_funcCallExpr instCondExpr(*instFunc, cond_args);

    BPatchSnippetHandle *handle =
            appBin->insertSnippet(instCondExpr, *instrumentPoint, BPatch_callBefore, BPatch_firstSnippet);
    if (!handle) {
            cerr << "Failed to insert instrumention in basic block at offset 0x" << hex << block_addr << endl;
            return false;
        }
    return true;         

}


/*
num_all_edges: the number of all edges
num_condition_edges: the number of all conditional edges
ind_addr_file: path to the file that contains (src_addr des_addr id)
*/
bool instrumentIndirect(BPatch_binaryEdit * appBin, BPatch_function * instFunc, 
                BPatch_point * instrumentPoint, Dyninst::Address src_addr, u32 edge_id){
    vector<BPatch_snippet *> ind_args;

    BPatch_constExpr EdgeID(edge_id);
    ind_args.push_back(&EdgeID);
    

    BPatch_funcCallExpr instIndirect(*instFunc, ind_args);

    BPatchSnippetHandle *handle =
            appBin->insertSnippet(instIndirect, *instrumentPoint, BPatch_callBefore, BPatch_firstSnippet);
    
    if (!handle) {
            cerr << "Failed to insert instrumention in basic block at offset 0x" << hex << src_addr << endl;
            return false;
        }
    return true;

}


/*instrument at edges
    addr_id_file: path to the file that contains (src_addr des_addr id)
*/
bool edgeInstrument(BPatch_binaryEdit * appBin, BPatch_image *appImage, 
                    vector < BPatch_function * >::iterator funcIter, char* funcName){
    BPatch_function *curFunc = *funcIter;
    BPatch_flowGraph *appCFG = curFunc->getCFG ();

    BPatch_Set < BPatch_basicBlock * > allBlocks;
    if (!appCFG->getAllBasicBlocks (allBlocks)) {
        cerr << "Failed to find basic blocks for function " << funcName << endl;
        return false;
    } else if (allBlocks.size () == 0) {
        cerr << "No basic blocks for function " << funcName << endl;
        return false;
    }

    set < BPatch_basicBlock *>::iterator bb_iter;
    for (bb_iter = allBlocks.begin (); bb_iter != allBlocks.end (); bb_iter++){
        BPatch_basicBlock * block = *bb_iter;
        vector<pair<Dyninst::InstructionAPI::Instruction, Dyninst::Address> > insns;
        block->getInstructions(insns);

        Dyninst::Address addr = insns.back().second;  //addr: equal to offset when it's binary rewrite
        Dyninst::InstructionAPI::Instruction insn = insns.back().first; 
        Dyninst::InstructionAPI::Operation op = insn.getOperation();
        Dyninst::InstructionAPI::InsnCategory category = insn.getCategory();
        Dyninst::InstructionAPI::Expression::Ptr expt = insn.getControlFlowTarget();

        //pre-determined edges
        vector<BPatch_edge *> outgoingEdge;
        (*bb_iter)->getOutgoingEdges(outgoingEdge);
        vector<BPatch_edge *>::iterator edge_iter;
        //u8 edge_type;

        for(edge_iter = outgoingEdge.begin(); edge_iter != outgoingEdge.end(); ++edge_iter) {
            //edge_type = (*edge_iter)->getType();
            if ((*edge_iter)->getType() == CondJumpTaken){
                instrumentCondition(appBin, ConditionJump, (*edge_iter)->getPoint(), addr, AllEdge_id);
                AllEdge_id++;
                if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
            }
            else if ((*edge_iter)->getType() == CondJumpNottaken){
                instrumentCondition(appBin, ConditionJump, (*edge_iter)->getPoint(), addr, AllEdge_id);
                AllEdge_id++;
                if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
            } 
            else if ((*edge_iter)->getType() == NonJump){
                instrumentCondition(appBin, ConditionJump, (*edge_iter)->getPoint(), addr, AllEdge_id);
                AllEdge_id++;
                if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
            } 
            else if ((*edge_iter)->getType() == UncondJump){
                instrumentCondition(appBin, ConditionJump, (*edge_iter)->getPoint(), addr, AllEdge_id);
                AllEdge_id++;
                if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
            }    
            
        }


        //indirect edges
        for(Dyninst::InstructionAPI::Instruction::cftConstIter iter = insn.cft_begin(); iter != insn.cft_end(); ++iter) {
            if(iter->isIndirect) {
                
                if(category == Dyninst::InstructionAPI::c_CallInsn) {//indirect call
                    vector<BPatch_point *> callPoints;
                    appImage->findPoints(addr, callPoints); //use callPoints[0] as the instrument point
                    instrumentIndirect(appBin, IndirectEdges, callPoints[0], addr,  AllEdge_id);
                    AllEdge_id++;
                    if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
                }
                else if(category == Dyninst::InstructionAPI::c_BranchInsn) {//indirect jump
                    vector<BPatch_point *> callPoints;
                    appImage->findPoints(addr, callPoints); //use callPoints[0] as the instrument point
                    instrumentIndirect(appBin, IndirectEdges, callPoints[0], addr, AllEdge_id);
                    AllEdge_id++;
                    if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
                                
                }
                else if(category == Dyninst::InstructionAPI::c_ReturnInsn) {
                    vector<BPatch_point *> retPoints;
                    appImage->findPoints(addr, retPoints);

                    instrumentIndirect(appBin, IndirectEdges, retPoints[0], addr, AllEdge_id);
                    AllEdge_id++;
                    if (AllEdge_id >= MAP_SIZE) AllEdge_id = random() % MAP_SIZE;
                }
 
            }
        }
    }
    return true;
}

/* insert forkserver at the beginning of main
    funcInit: function to be instrumented, i.e., main

*/

bool insertForkServer(BPatch_binaryEdit * appBin, BPatch_function * instIncFunc,
                         BPatch_function *funcInit)
{

    /* Find the instrumentation points */
    vector < BPatch_point * >*funcEntry = funcInit->findPoint (BPatch_entry);

    if (NULL == funcEntry) {
        cerr << "Failed to find entry for function. " <<  endl;
        return false;
    }

    //cout << "Inserting init callback." << endl;
    BPatch_Vector < BPatch_snippet * >instArgs; 

    BPatch_funcCallExpr instIncExpr(*instIncFunc, instArgs);

    /* Insert the snippet at function entry */
    BPatchSnippetHandle *handle =
        appBin->insertSnippet (instIncExpr, *funcEntry, BPatch_callBefore, BPatch_firstSnippet);
    if (!handle) {
        cerr << "Failed to insert init callback." << endl;
        return false;
    }
    return true;
}

int main (int argc, char **argv){

     if(!parseOptions(argc,argv)) {
        return EXIT_FAILURE;
    }

    /* start instrumentation*/
    BPatch bpatch;
    // skip all libraries unless -l is set
    BPatch_binaryEdit *appBin = bpatch.openBinary (originalBinary, false);
    if (appBin == NULL) {
        cerr << "Failed to open binary" << endl;
        return EXIT_FAILURE;
    }

    // if(!instrumentLibraries.empty()){
    //     for(auto lbit = instrumentLibraries.begin(); lbit != instrumentLibraries.end(); lbit++){
    //         if (!appBin->loadLibrary ((*lbit).c_str())) {
    //             cerr << "Failed to open instrumentation library " << *lbit << endl;
    //             cerr << "It needs to be located in the current working directory." << endl;
    //             return EXIT_FAILURE;
    //         }
    //     }
    // }

    BPatch_image *appImage = appBin->getImage ();

    
    vector < BPatch_function * > allFunctions;
    appImage->getProcedures(allFunctions);

    if (!appBin->loadLibrary (instLibrary)) {
        cerr << "Failed to open instrumentation library " << instLibrary << endl;
        cerr << "It needs to be located in the current working directory." << endl;
        return EXIT_FAILURE;
    }

    initAflForkServer = findFuncByName (appImage, (char *) "initAflForkServer");
 
    ConditionJump = findFuncByName (appImage, (char *) "ConditionJump");
    IndirectEdges = findFuncByName (appImage, (char *) "IndirectEdges");


    if (!initAflForkServer || !ConditionJump || !IndirectEdges) {
        cerr << "Instrumentation library lacks callbacks!" << endl;
        return EXIT_FAILURE;
    }


   /* instrument edges */
    vector < BPatch_function * >::iterator funcIter;
    for (funcIter = allFunctions.begin (); funcIter != allFunctions.end (); ++funcIter) {
        BPatch_function *curFunc = *funcIter;
        char funcName[1024];
        curFunc->getName (funcName, 1024);
        if(isSkipFuncs(funcName)) continue;
        //instrument at edges
        //if(!edgeInstrument(appBin, appImage, funcIter, funcName, addr_id_file)) return EXIT_FAILURE;
        edgeInstrument(appBin, appImage, funcIter, funcName);

    }

    BPatch_function *funcToPatch = NULL;
    BPatch_Vector<BPatch_function*> funcs;
    
    appImage->findFunction("main",funcs);
    if(!funcs.size()) {
        cerr << "Couldn't locate main, check your binary. "<< endl;
        return EXIT_FAILURE;
    }
    // there should really be only one
    funcToPatch = funcs[0];

    if(!insertForkServer (appBin, initAflForkServer, funcToPatch)){
        cerr << "Could not insert init callback at main." << endl;
        return EXIT_FAILURE;
    }

    if(verbose){
        cout << "Saving the instrumented binary to " << instrumentedBinary << "..." << endl;
    }
    // save the instrumented binary
    if (!appBin->writeFile (instrumentedBinary)) {
        cerr << "Failed to write output file: " << instrumentedBinary << endl;
        return EXIT_FAILURE;
    }

    if(verbose){
        cout << "All done! Happy fuzzing!" << endl;
    }

    return EXIT_SUCCESS;


}
