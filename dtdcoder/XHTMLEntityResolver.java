import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;

import java.net.URLConnection;
import java.net.URL;

import java.util.HashMap;
import java.util.Map;

import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;

/**
 * Workaround for W3C blocking requests with the User-Agent
 * header the XML parser sets.
 *
 */
public class XHTMLEntityResolver implements EntityResolver {
    String baseURL;
    String sep;
    Map cache;

    public XHTMLEntityResolver() {
        if (File.separatorChar == '\\') {
            sep = "\\\\";
        } else {
            sep = File.separator;
        }

        cache = new HashMap();
    }

    public void setBaseURL(String dtdURL) {
        baseURL = dtdURL.replaceAll("/[^/]*\\z", "/");
    }

    public InputSource resolveEntity (String publicId, String systemId) {
        String resolvedUrl;
        if (!systemId.startsWith("http://")) {
            String end = systemId.replaceAll("\\A.*" + sep, "");
            if (publicId.startsWith("-//W3C//ENTITIES XHTML 1.1")
                || publicId.startsWith("-//W3C//ENTITIES XHTML-Print 1.0")) {
                resolvedUrl = baseURL + end;
            } else if (publicId.startsWith("-//W3C//ENTITIES")
                       || publicId.startsWith("-//W3C//ELEMENTS")) {
                resolvedUrl = "http://www.w3.org/TR/xhtml-modularization/DTD/"
                    + end;
            } else {
                resolvedUrl = baseURL + end;
            }
        } else {
            resolvedUrl = systemId;
        }

        return getResource(resolvedUrl);
    }

    private InputSource getResource(String url) {
        InputSource source = null;

        if (!cache.containsKey(url)) {
            System.err.println("Downloading " + url);
            try {
                URLConnection con = (new URL(url)).openConnection();
                con.setRequestProperty("User-Agent", "html2xhtml/DTDCoder");
                InputStream is = con.getInputStream();
                ByteArrayOutputStream os = new ByteArrayOutputStream();
                byte[] buffer = new byte[1024];
                int read = 0;
                while (read != -1) {
                    read = is.read(buffer);
                    if (read > 0) {
                        os.write(buffer, 0, read);
                    }
                }
                os.close();
                cache.put(url, os.toByteArray());

            } catch (IOException ioe) {
                ioe.printStackTrace();
            }
        }

        byte[] data = (byte[]) cache.get(url);
        source = new InputSource(new ByteArrayInputStream(data));

        return source;
    }
}
