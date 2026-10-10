// Disassemble and create functions at every address listed in a file (one hex address per line).
// Usage: -postScript CreateFuncs.java <file>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class CreateFuncs extends GhidraScript {
    public void run() throws Exception {
        int made = 0, existed = 0, failed = 0;
        try (BufferedReader r = new BufferedReader(new FileReader(getScriptArgs()[0]))) {
            String l;
            while ((l = r.readLine()) != null) {
                l = l.trim(); if (l.isEmpty()) continue;
                Address a = toAddr(l);
                if (getFunctionAt(a) != null) { existed++; continue; }
                Function c = getFunctionContaining(a);
                if (c != null) {
                    // address is inside an existing (too large) function: split by creating a new one
                    println("inside " + c.getName() + " : " + l);
                }
                disassemble(a);
                Function f = createFunction(a, null);
                if (f == null) { failed++; println("FAILED " + l); } else made++;
            }
        }
        println("created=" + made + " existed=" + existed + " failed=" + failed);
    }
}
