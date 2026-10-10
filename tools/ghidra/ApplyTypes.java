// Parse a C header into the program, set external function signatures, and type globals.
// Usage: -postScript ApplyTypes.java <header.h> <globals.tsv>
// globals.tsv lines: address \t typename \t pointer_depth \t array_count \t [name]
import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.util.*;
import java.io.*;
import java.util.*;

public class ApplyTypes extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        CParser parser = new CParser(dtm, true, null);
        Map<String, FunctionDefinition> protos = new HashMap<>();
        try (InputStream in = new FileInputStream(a[0])) {
            parser.parse(in);
        }
        for (Map.Entry<String, DataType> e : parser.getFunctions().entrySet())
            if (e.getValue() instanceof FunctionDefinition) protos.put(e.getKey(), (FunctionDefinition) e.getValue());
        println("parsed types; functions in header: " + protos.keySet());

        // external function signatures (DSOUND ordinal 1 == DirectSoundCreate)
        for (Function f : currentProgram.getFunctionManager().getExternalFunctions()) {
            String n = f.getName();
            String lib = f.getExternalLocation() != null ? f.getExternalLocation().getLibraryName() : "";
            String key = n;
            if (lib.equalsIgnoreCase("DSOUND.DLL") && n.equals("Ordinal_1")) key = "DirectSoundCreate";
            FunctionDefinition fd = protos.get(key);
            if (fd == null) continue;
            if (!key.equals(n)) f.setName(key, SourceType.USER_DEFINED);
            boolean ok = new ApplyFunctionSignatureCmd(f.getEntryPoint(), fd, SourceType.USER_DEFINED).applyTo(currentProgram);
            println("signature " + key + " -> " + ok);
        }

        if (a.length > 1) {
            try (BufferedReader r = new BufferedReader(new FileReader(a[1]))) {
                String line;
                while ((line = r.readLine()) != null) {
                    line = line.trim();
                    if (line.isEmpty() || line.startsWith("#")) continue;
                    String[] p = line.split("\t");
                    Address addr = toAddr(p[0]);
                    DataType base = findType(dtm, p[1]);
                    if (base == null) { println("NO TYPE " + p[1]); continue; }
                    int depth = Integer.parseInt(p[2]);
                    int count = Integer.parseInt(p[3]);
                    DataType dt = base;
                    for (int i = 0; i < depth; i++) dt = new PointerDataType(dt, 4, dtm);
                    if (count > 1) dt = new ArrayDataType(dt, count, dt.getLength(), dtm);
                    DataUtilities.createData(currentProgram, addr, dt, -1, DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA);
                    if (p.length > 4 && !p[4].isEmpty())
                        currentProgram.getSymbolTable().createLabel(addr, p[4], SourceType.USER_DEFINED).setPrimary();
                    println("typed " + p[0] + " " + dt.getDisplayName() + (p.length > 4 ? " " + p[4] : ""));
                }
            }
        }
    }

    private DataType findType(DataTypeManager dtm, String name) {
        List<DataType> l = new ArrayList<>();
        dtm.findDataTypes(name, l);
        for (DataType d : l) if (!(d instanceof Pointer)) return d;
        return l.isEmpty() ? null : l.get(0);
    }
}
