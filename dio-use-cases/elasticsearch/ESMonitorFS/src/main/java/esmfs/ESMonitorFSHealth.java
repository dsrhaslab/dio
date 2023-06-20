package esmfs;

import java.util.Set;
import java.io.OutputStream;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import org.elasticsearch.core.IOUtils;

class ESMonitorFSHealth implements Runnable {
    static final String TEMP_FILE_NAME = "es_temp_file";
    private byte[] bytesToWrite;

    ESMonitorFSHealth () {
        this.bytesToWrite = "this is a test".getBytes();
    }

    @Override
    public void run() {
        monitorFSHealth();
        System.out.println("health check succeeded");
    }

    private void monitorFSHealth(){
        final Path path = Paths.get("tmp");
        try {
            char ch = (char) System.in.read();
            final Path tempDataPath = path.resolve(TEMP_FILE_NAME);
            System.out.println("tempDataPath: " + tempDataPath);
            ch = (char) System.in.read();

            Files.deleteIfExists(tempDataPath);
            System.out.println("After deleteIfExists");
            ch = (char) System.in.read();

            try (OutputStream os = Files.newOutputStream(tempDataPath, StandardOpenOption.CREATE_NEW)) {
                System.out.println("After newOutputStream");
                ch = (char) System.in.read();

                os.write(bytesToWrite);
                System.out.println("After write");
                ch = (char) System.in.read();

                IOUtils.fsync(tempDataPath, false);
                System.out.println("After fsync");
                ch = (char) System.in.read();
            }
            Files.delete(tempDataPath);
            System.out.println("After delete");
            ch = (char) System.in.read();
        } catch (Exception ex) {
            System.out.println("health check of [" + path + "] failed:" + ex);
        }
    }

}