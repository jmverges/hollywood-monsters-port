// Write every data type of the program as C (typedefs, structs, unions, enums, function definitions).
// Usage: -postScript ExportTypes.java <out.h>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import java.io.*;
import java.util.*;
public class ExportTypes extends GhidraScript {
    public void run() throws Exception {
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        List<DataType> all = new ArrayList<>();
        Iterator<DataType> it = dtm.getAllDataTypes();
        while (it.hasNext()) all.add(it.next());
        try (FileWriter fw = new FileWriter(getScriptArgs()[0])) {
            DataTypeWriter w = new DataTypeWriter(dtm, fw);
            w.write(all, monitor);
        }
        println("types written: " + all.size());
    }
}
