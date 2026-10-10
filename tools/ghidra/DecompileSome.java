// Decompile the given function entry addresses and write them to <outdir>/<addr>.c
// Usage: -postScript DecompileSome.java <outdir> addr1 addr2 ...
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class DecompileSome extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        File out = new File(a[0]); out.mkdirs();
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        for (int i = 1; i < a.length; i++) {
            Function f = getFunctionAt(toAddr(a[i]));
            if (f == null) { println("no function at " + a[i]); continue; }
            DecompileResults r = di.decompileFunction(f, 180, monitor);
            try (PrintWriter w = new PrintWriter(new FileWriter(new File(out, a[i] + ".c")))) {
                w.print(r.getDecompiledFunction().getC());
            }
        }
    }
}
