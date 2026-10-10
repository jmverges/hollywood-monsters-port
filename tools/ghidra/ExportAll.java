// Export every function of the current program: metadata TSV, decompiled C, call graph, strings.
// Usage (headless): -postScript ExportAll.java <outdir>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.*;
import java.io.*;
import java.util.*;

public class ExportAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : "/tmp/ghidra-export");
        File cdir = new File(out, "decomp");
        cdir.mkdirs();
        Listing listing = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();

        DecompInterface di = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        di.setOptions(opts);
        di.toggleCCode(true);
        di.toggleSyntaxTree(false);
        di.setSimplificationStyle("decompile");
        di.openProgram(currentProgram);

        PrintWriter tsv = new PrintWriter(new FileWriter(new File(out, "functions.tsv")));
        tsv.println("entry\tname\tsize\tthunk\texternal\tcallers\tcallees\tsource\tsignature");
        PrintWriter calls = new PrintWriter(new FileWriter(new File(out, "callgraph.tsv")));
        PrintWriter strs = new PrintWriter(new FileWriter(new File(out, "func_strings.tsv")));

        int n = 0, failed = 0;
        for (Function f : fm.getFunctions(true)) {
            if (monitor.isCancelled()) break;
            Address e = f.getEntryPoint();
            Set<Function> callers = f.getCallingFunctions(monitor);
            Set<Function> callees = f.getCalledFunctions(monitor);
            long size = f.getBody().getNumAddresses();
            tsv.println(e + "\t" + f.getName() + "\t" + size + "\t" + f.isThunk() + "\t" + f.isExternal()
                + "\t" + callers.size() + "\t" + callees.size() + "\t" + f.getSymbol().getSource()
                + "\t" + f.getSignature().getPrototypeString().replace('\t', ' '));
            for (Function c : callees) calls.println(e + "\t" + c.getEntryPoint() + "\t" + c.getName());
            // strings referenced from this function body
            for (Address a : f.getBody().getAddresses(true)) {
                for (Reference r : rm.getReferencesFrom(a)) {
                    Data d = listing.getDataAt(r.getToAddress());
                    if (d != null && d.hasStringValue()) {
                        String s = String.valueOf(d.getValue()).replace('\n', ' ').replace('\t', ' ').replace('\r', ' ');
                        strs.println(e + "\t" + r.getToAddress() + "\t" + s);
                    }
                }
            }
            if (f.isExternal() || f.isThunk()) continue;
            DecompileResults res = di.decompileFunction(f, 120, monitor);
            if (res != null && res.decompileCompleted()) {
                try (PrintWriter w = new PrintWriter(new FileWriter(new File(cdir, e.toString() + ".c")))) {
                    w.println("// " + f.getName() + " @ " + e + " size=" + size);
                    w.print(res.getDecompiledFunction().getC());
                }
            } else {
                failed++;
            }
            n++;
            if (n % 500 == 0) println("decompiled " + n);
        }
        tsv.close(); calls.close(); strs.close();
        // data types used / globals summary
        PrintWriter g = new PrintWriter(new FileWriter(new File(out, "imports.tsv")));
        for (Symbol s : currentProgram.getSymbolTable().getExternalSymbols())
            g.println(s.getParentNamespace().getName() + "\t" + s.getName());
        g.close();
        println("DONE functions=" + n + " failed=" + failed);
    }
}
