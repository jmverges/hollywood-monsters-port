// Export every symbol located in non-executable memory: name, address, data type (as C declarator), length.
// Usage: -postScript ExportSymbols.java <out.tsv>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.data.*;
import java.io.*;
public class ExportSymbols extends GhidraScript {
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        Listing listing = currentProgram.getListing();
        int n = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(getScriptArgs()[0]))) {
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (s.isExternal()) continue;
                MemoryBlock b = mem.getBlock(s.getAddress());
                if (b == null || b.isExecute()) continue;
                Data d = listing.getDataAt(s.getAddress());
                String decl = "undefined @"; int len = 1;
                if (d != null && d.getDataType() != null && !(d.getDataType() instanceof DefaultDataType)) {
                    decl = ExportRecomp.decl(d.getDataType(), "@"); len = d.getLength();
                }
                w.println(s.getName() + "\t" + s.getAddress() + "\t" + decl + "\t" + len);
                n++;
            }
        }
        println("symbols=" + n);
    }
}
