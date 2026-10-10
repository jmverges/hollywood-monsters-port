// Name CRT helpers and give FPU intrinsics custom register storage so call sites show their arguments.
// Usage: -postScript SetSigs.java
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
public class SetSigs extends GhidraScript {
    void rename(String addr, String name) throws Exception {
        Function f = getFunctionAt(toAddr(addr));
        if (f == null) { println("no func " + addr); return; }
        f.setName(name, SourceType.USER_DEFINED);
    }
    public void run() throws Exception {
        rename("00537ec0", "_free");
        rename("0053a710", "_srand");
        rename("0053cbc0", "_ceil");
        rename("0053cce0", "_floor");
        Register st0 = currentProgram.getRegister("ST0");
        Register eax = currentProgram.getRegister("EAX"), edx = currentProgram.getRegister("EDX");
        // _CIsqrt: double in ST0 -> double in ST0
        Function f = getFunctionAt(toAddr("0053d1d0"));
        f.setName("_CIsqrt", SourceType.USER_DEFINED);
        Parameter p = new ParameterImpl("x", DoubleDataType.dataType, new VariableStorage(currentProgram, st0), currentProgram);
        ReturnParameterImpl r = new ReturnParameterImpl(DoubleDataType.dataType, new VariableStorage(currentProgram, st0), currentProgram);
        f.updateFunction("__cdecl", r, Function.FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED, p);
        // __ftol2: double in ST0 -> longlong in EDX:EAX
        f = getFunctionAt(toAddr("0053c350"));
        f.setName("__ftol2", SourceType.USER_DEFINED);
        p = new ParameterImpl("x", DoubleDataType.dataType, new VariableStorage(currentProgram, st0), currentProgram);
        r = new ReturnParameterImpl(LongLongDataType.dataType, new VariableStorage(currentProgram, edx, eax), currentProgram);
        f.updateFunction("__cdecl", r, Function.FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED, p);
        String[][] simple = {
            {"0053cbc0", "double"}, {"0053cce0", "double"}
        };
        for (String[] sp : simple) {
            Function g = getFunctionAt(toAddr(sp[0]));
            Parameter q = new ParameterImpl("x", DoubleDataType.dataType, currentProgram);
            g.updateFunction("__cdecl", new ReturnParameterImpl(DoubleDataType.dataType, currentProgram),
                Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED, q);
        }
        Function g = getFunctionAt(toAddr("00537ec0"));
        g.updateFunction("__cdecl", new ReturnParameterImpl(VoidDataType.dataType, currentProgram),
            Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED,
            new ParameterImpl("p", new PointerDataType(VoidDataType.dataType), currentProgram));
        g = getFunctionAt(toAddr("0053a710"));
        g.updateFunction("__cdecl", new ReturnParameterImpl(VoidDataType.dataType, currentProgram),
            Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED,
            new ParameterImpl("seed", UnsignedIntegerDataType.dataType, currentProgram));
        println("signatures set");
    }
}
