// Calling conventions of functions that end in a tail jump instead of a RET.
//
// Ghidra takes a callee's stack purge from the RET N in its own body. Functions without one (a 5-byte JMP stub
// not recognised as a thunk, or a function whose last instruction jumps into another) fall back to the
// __stdcall default of 4 bytes per parameter, although the code they jump to returns with a plain RET and the
// caller cleans the stack. Every esp-relative local after such a call is then named a few bytes off: tables
// read from the wrong place, stores dropped as dead (the agenda and Quasimodo bugs). This script follows each
// such function's tail jumps to the RET that really ends the call and sets the function's purge to match.
// Usage: -postScript FixTailPurge.java
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.Scalar;

public class FixTailPurge extends GhidraScript {
    /** purge of the RET that ends a call into f (following tail jumps), or -1 if unknown */
    int realPurge(Function f, int depth) {
        if (f == null || depth > 8) return -1;
        Listing listing = currentProgram.getListing();
        int found = -2;
        for (Instruction in : listing.getInstructions(f.getBody(), true)) {
            String m = in.getMnemonicString();
            if (m.equals("RET")) {
                int n = 0;
                if (in.getNumOperands() > 0) {
                    Scalar s = in.getScalar(0);
                    if (s != null) n = (int) s.getUnsignedValue();
                }
                if (found == -2) found = n;
                else if (found != n) return -1;
            }
        }
        if (found >= 0) return found;
        // no RET: follow an unconditional JMP that leaves the function
        if (f.getBody().isEmpty()) return -1;
        Instruction last = listing.getInstructionContaining(f.getBody().getMaxAddress());
        if (last == null || !last.getMnemonicString().equals("JMP")) return -1;
        Address[] flows = last.getFlows();
        if (flows.length != 1 || f.getBody().contains(flows[0])) return -1;
        return realPurge(getFunctionAt(flows[0]), depth + 1);
    }

    boolean hasRet(Function f) {
        for (Instruction in : currentProgram.getListing().getInstructions(f.getBody(), true))
            if (in.getMnemonicString().equals("RET")) return true;
        return false;
    }

    public void run() throws Exception {
        int changed = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isExternal() || f.isThunk() || hasRet(f)) continue;
            int purge = realPurge(f, 0);
            if (purge < 0 || f.getStackPurgeSize() == purge) continue;
            // only the purge: the convention also decides where parameters live (__fastcall: ECX, EDX)
            println("tail purge: " + f.getName() + " @ " + f.getEntryPoint() + " (" + f.getCallingConventionName()
                    + ") purge " + f.getStackPurgeSize() + " -> " + purge);
            f.setStackPurgeSize(purge);
            changed++;
        }
        println("tail purge: " + changed + " functions fixed");
    }
}
