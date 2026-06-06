package com.nettest;
import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
public class NetTest implements Runnable {
    public static void run2() { try { new Thread(new NetTest()).start(); } catch (Throwable t) {} }
    public void run() {
        log("=== NetTest start ===");
        test("https://api.trynoice.com/v1/subscriptions/plans");
        test("https://cdn.trynoice.com/library/library.json");
        log("=== NetTest done ===");
    }
    static void test(String url) {
        try {
            URL u = new URL(url);
            HttpsURLConnection c = (HttpsURLConnection) u.openConnection();
            c.setConnectTimeout(12000); c.setReadTimeout(12000);
            int code = c.getResponseCode();
            log("OK " + url + " -> HTTP " + code);
            c.disconnect();
        } catch (Throwable t) {
            StringWriter sw = new StringWriter();
            t.printStackTrace(new PrintWriter(sw));
            log("FAIL " + url + " -> " + t.getClass().getName() + ": " + t.getMessage() + "\n" + sw.toString());
        }
    }
    static void log(String s) {
        try { FileWriter w = new FileWriter("/data/local/tmp/httptest.log", true); w.write(s + "\n"); w.close(); } catch (Throwable e) {}
    }
}
