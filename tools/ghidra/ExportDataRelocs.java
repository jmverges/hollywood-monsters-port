// Exports every reference from initialised, non-executable memory to an address inside the image: the 4-byte
// words that hold absolute addresses and must be adjusted when the image is loaded somewhere other than its
// original base (the executable has no relocation table). Output: datarelocs.tsv
//   source address \t target address \t source block \t reference type \t data type at source
//@category Recomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class ExportDataRelocs extends GhidraScript {
    @Override
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        PrintWriter w = new PrintWriter(new FileWriter(out));
        ReferenceManager rm = currentProgram.getReferenceManager();
        Memory mem = currentProgram.getMemory();
        Listing listing = currentProgram.getListing();
        long n = 0;
        for (MemoryBlock b : mem.getBlocks()) {
            if (b.isExecute() || !b.isInitialized()) continue;
            AddressIterator ai = rm.getReferenceSourceIterator(b.getStart(), true);
            while (ai.hasNext()) {
                Address a = ai.next();
                if (a.compareTo(b.getEnd()) > 0) break;
                for (Reference r : rm.getReferencesFrom(a)) {
                    Address to = r.getToAddress();
                    if (!to.isMemoryAddress() || mem.getBlock(to) == null) continue;
                    Data d = listing.getDataContaining(a);
                    String dt = d == null ? "" : d.getDataType().getName();
                    w.println(a + "\t" + to + "\t" + b.getName() + "\t" + r.getReferenceType() + "\t" + dt);
                    n++;
                }
            }
        }
        w.close();
        println("datarelocs: " + n);
    }
}
