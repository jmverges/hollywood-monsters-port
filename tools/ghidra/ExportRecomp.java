// Export everything the recompilation pipeline needs.
//  funcs/<entry>.c        decompiled C of each non-thunk function
//  funcs/<entry>.vars     token \t address \t C declarator template (with @ as the name placeholder)
//  functions.tsv          entry, name, size, thunk, thunk_target, external, callconv, return decl, signature
//  dataptrs.tsv           data address \t target address \t containing function entry \t block
// Usage: -postScript ExportRecomp.java <outdir> [minAddr maxAddr]
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public class ExportRecomp extends GhidraScript {

    static String decl(DataType dt, String inner) {
        if (dt instanceof TypeDef) return dt.getName() + " " + inner;
        if (dt instanceof Pointer) {
            DataType p = ((Pointer) dt).getDataType();
            String in = "*" + inner;
            if (p == null) return "void " + in;
            if (p instanceof Array || p instanceof FunctionDefinition) in = "(" + in + ")";
            return decl(p, in);
        }
        if (dt instanceof Array) {
            Array a = (Array) dt;
            return decl(a.getDataType(), inner + "[" + a.getNumElements() + "]");
        }
        if (dt instanceof FunctionDefinition) {
            FunctionDefinition fd = (FunctionDefinition) dt;
            StringBuilder sb = new StringBuilder();
            ParameterDefinition[] ps = fd.getArguments();
            for (int i = 0; i < ps.length; i++) {
                if (i > 0) sb.append(", ");
                sb.append(decl(ps[i].getDataType(), ""));
            }
            if (fd.hasVarArgs()) sb.append(ps.length > 0 ? ", ..." : "...");
            if (ps.length == 0 && !fd.hasVarArgs()) sb.append("void");
            return decl(fd.getReturnType(), inner + "(" + sb + ")");
        }
        if (dt instanceof Structure) return "struct " + dt.getName() + " " + inner;
        if (dt instanceof Union) return "union " + dt.getName() + " " + inner;
        if (dt instanceof ghidra.program.model.data.Enum) return dt.getName() + " " + inner;
        return dt.getName() + " " + inner;
    }

    void collect(ClangNode n, Map<String, String[]> vars) {
        if (n instanceof ClangVariableToken) {
            ClangVariableToken t = (ClangVariableToken) n;
            HighVariable hv = t.getHighVariable();
            if (hv instanceof HighGlobal || (hv != null && hv.getSymbol() != null && hv.getSymbol().isGlobal())) {
                String txt = t.getText();
                if (!vars.containsKey(txt)) {
                    String addr = "";
                    try {
                        HighSymbol hs = hv.getSymbol();
                        if (hs != null && hs.getStorage() != null && hs.getStorage().getMinAddress() != null)
                            addr = hs.getStorage().getMinAddress().toString();
                    } catch (Exception e) { }
                    String repAddr = "";
                    Varnode rep = hv.getRepresentative();
                    if (rep != null && rep.getAddress() != null && rep.getAddress().isMemoryAddress())
                        repAddr = rep.getAddress().toString();
                    if (!repAddr.isEmpty()) addr = repAddr;   // the accessed address (may be inside a larger symbol)
                    if (addr.isEmpty() && hv.getRepresentative() != null && hv.getRepresentative().getAddress() != null)
                        addr = hv.getRepresentative().getAddress().toString();
                    DataType dt = null; int size = hv.getSize();
                    HighSymbol hs2 = hv.getSymbol();
                    if (hs2 != null && hs2.getDataType() != null && hs2.getSize() >= hv.getSize()) { dt = hs2.getDataType(); size = hs2.getSize(); }
                    if (dt == null) dt = hv.getDataType();
                    vars.put(txt, new String[] { addr, dt == null ? "undefined4 @" : decl(dt, "@"), String.valueOf(size) });
                }
            }
        }
        if (n instanceof ClangTokenGroup) {
            ClangTokenGroup g = (ClangTokenGroup) n;
            for (int i = 0; i < g.numChildren(); i++) collect(g.Child(i), vars);
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        Address lo = args.length > 2 ? toAddr(args[1]) : null, hi = args.length > 2 ? toAddr(args[2]) : null;
        File fdir = new File(out, "funcs");
        fdir.mkdirs();
        FunctionManager fm = currentProgram.getFunctionManager();
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.toggleCCode(true);
        di.toggleSyntaxTree(true);
        di.setSimplificationStyle("decompile");
        di.openProgram(currentProgram);

        PrintWriter tsv = new PrintWriter(new FileWriter(new File(out, "functions.tsv")));
        tsv.println("entry\tname\tsize\tthunk\tthunk_target\texternal\tcallconv\tret\tsignature");
        int n = 0, failed = 0;
        for (Function f : fm.getFunctions(true)) {
            if (monitor.isCancelled()) break;
            Address e = f.getEntryPoint();
            String tt = "";
            if (f.isThunk()) {
                Function t = f.getThunkedFunction(true);
                if (t != null) tt = t.isExternal() ? ("EXT:" + t.getName()) : t.getEntryPoint().toString();
            }
            tsv.println(e + "\t" + f.getName() + "\t" + f.getBody().getNumAddresses() + "\t" + f.isThunk() + "\t" + tt
                + "\t" + f.isExternal() + "\t" + f.getCallingConventionName() + "\t" + decl(f.getReturnType(), "@")
                + "\t" + f.getSignature().getPrototypeString().replace('\t', ' '));
            if (f.isExternal() || f.isThunk()) continue;
            if (lo != null && (e.compareTo(lo) < 0 || e.compareTo(hi) >= 0)) continue;
            DecompileResults res = di.decompileFunction(f, 180, monitor);
            if (res == null || !res.decompileCompleted()) { failed++; println("FAILED " + e); continue; }
            try (PrintWriter w = new PrintWriter(new FileWriter(new File(fdir, e.toString() + ".c")))) {
                w.print(res.getDecompiledFunction().getC());
            }
            Map<String, String[]> vars = new TreeMap<>();
            collect(res.getCCodeMarkup(), vars);
            try (PrintWriter w = new PrintWriter(new FileWriter(new File(fdir, e.toString() + ".vars")))) {
                for (Map.Entry<String, String[]> v : vars.entrySet())
                    w.println(v.getKey() + "\t" + v.getValue()[0] + "\t" + v.getValue()[1] + "\t" + v.getValue()[2]);
            }
            n++;
            if (n % 500 == 0) println("decompiled " + n);
        }
        for (Function f : fm.getExternalFunctions()) {
            tsv.println("EXTERNAL:" + f.getName() + "\t" + f.getName() + "\t0\tfalse\t\ttrue\t" + f.getCallingConventionName()
                + "\t" + decl(f.getReturnType(), "@") + "\t" + f.getSignature().getPrototypeString().replace('\t', ' '));
        }
        tsv.close();

        PrintWriter dp = new PrintWriter(new FileWriter(new File(out, "dataptrs.tsv")));
        ReferenceManager rm = currentProgram.getReferenceManager();
        Memory mem = currentProgram.getMemory();
        for (MemoryBlock b : mem.getBlocks()) {
            if (b.isExecute() || !b.isInitialized()) continue;
            AddressIterator ai = rm.getReferenceSourceIterator(b.getStart(), true);
            while (ai.hasNext()) {
                Address a = ai.next();
                if (a.compareTo(b.getEnd()) > 0) break;
                for (Reference r : rm.getReferencesFrom(a)) {
                    Address to = r.getToAddress();
                    MemoryBlock tb = mem.getBlock(to);
                    if (tb == null || !tb.isExecute()) continue;
                    Function tf = fm.getFunctionContaining(to);
                    dp.println(a + "\t" + to + "\t" + (tf == null ? "" : tf.getEntryPoint().toString()) + "\t" + b.getName());
                }
            }
        }
        dp.close();
        println("DONE functions=" + n + " failed=" + failed);
    }
}
