/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.awt.*;
import java.awt.image.BufferedImage;
import java.math.BigDecimal;
import javax.imageio.*;

// A class to generate frames for a 2 hour test pattern video.
// Uncompressed BMP images are streamed to stdout and are expected to be
// piped to a file for later processing into an mp4.
public class Visual extends Frame {
    int SIZEX;
    int SIZEY;

    // 16 x 9 for grid
    double GRID_SIZE;

    static int FPS = 30;
    static double FPS_D = 30.0d;
    static int NUMFRAMES = FPS * 60 * 60 * 2; // 2 hours

    Font smallFont;
    Font timecodeFont;
    Font resolutionFont;

    boolean outputFrames = true;

    BufferedImage image;

    public static void main(String args[]) {
        int h = Integer.parseInt(args[0]);
        int w = 0;
        if (h == 480) {
            w = 854;
        } else if (h == 720) {
            w = 1280;
        } else if (h == 1080) {
            w = 1920;
        } else {
            System.exit(-1);
        }

        Visual vis = new Visual(w, h);
        vis.smallFont = new Font("Arial", Font.BOLD, 14);
        vis.timecodeFont = new Font("Arial", Font.BOLD, 24);
        vis.resolutionFont = new Font("Arial", Font.BOLD, 36);
        vis.setSize(w, h);
        vis.setVisible(true);
        vis.animate();
    }

    public Visual(int w, int h) {
        SIZEX = w;
        SIZEY = h;
        GRID_SIZE = h / 9;
        image = new BufferedImage(SIZEX, SIZEY, BufferedImage.TYPE_INT_RGB);
    }

    public void paint(Graphics g) {
        g.drawImage(image, 0, 0, null);
    }

    public void update(Graphics g) {
        paint(g);
    }

    public void animate() {
        Graphics2D g2d = (Graphics2D) image.getGraphics();
        g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                RenderingHints.VALUE_ANTIALIAS_ON);

        for (int i = 0; i <= NUMFRAMES; i++) {
            framePolicy(g2d, i);
            writeFrame(image, i);
            repaint();
        }
        System.exit(0);
    }

    public void writeFrame(BufferedImage image, int frame) {
        if (!outputFrames)
            return;
        try {
            ImageIO.write(image, "bmp", System.out);
            System.out.flush();
        } catch (Exception je) {
            System.exit(-1);
        }
    }

    public void framePolicy(Graphics2D g, int frame) {

        BigDecimal var3600 = new BigDecimal(3600);
        BigDecimal var60 = new BigDecimal(60);
        BigDecimal totalSeconds = new BigDecimal((double) frame / FPS_D);
        BigDecimal hours = totalSeconds.divide(var3600, BigDecimal.ROUND_FLOOR);
        BigDecimal myremainder = totalSeconds.remainder(var3600);
        BigDecimal minutes = myremainder.divide(var60, BigDecimal.ROUND_FLOOR);
        BigDecimal seconds = myremainder.remainder(var60);

        g.setColor(Color.darkGray);
        g.clearRect(0, 0, SIZEX, SIZEY);

        // Grid.
        g.setColor(Color.white);
        g.setStroke(new BasicStroke(1));
        for (double x = 0; x < SIZEX; x += GRID_SIZE) {
            g.drawLine((int) x, 0, (int) x, SIZEY);
        }
        g.drawLine(SIZEX - 1, 0, SIZEX - 1, SIZEY);
        for (double y = 0; y < SIZEY; y += GRID_SIZE) {
            g.drawLine(0, (int) y, SIZEX, (int) y);
        }
        g.drawLine(0, SIZEY - 1, SIZEX, SIZEY - 1);

        int armThick = SIZEY / 90;

        String str;

        // Hands will start at 12 O'clock, centered according to thickness.
        int startArc = 90 - armThick/2;
        int numTickMarks = 360 / 60;

        // Large circle.
        int stroke = SIZEY / 120;
        g.setStroke(new BasicStroke(stroke));
        int x = SIZEX / 2 - SIZEY / 2 + stroke;
        int y = stroke;
        int w = SIZEY - stroke * 2;
        int h = SIZEY - stroke * 2;
        Rectangle rect = new Rectangle(x, y, w, h);
        g.setColor(Color.white);
        g.drawOval(rect.x, rect.y, rect.width, rect.height);
        g.setColor(Color.red);
        // Seconds - 1 tick per second.
        g.fillArc(rect.x, rect.y, rect.width, rect.height,
                startArc - (frame / FPS * numTickMarks) % 360, armThick);

        str = "S:" + seconds.intValue();
        g.setFont(smallFont);
        g.setColor(Color.gray);
        g.drawString(str, rect.x
                + rect.width
                / 2
                - (int) (g.getFontMetrics().getStringBounds(str, null)
                        .getWidth() / 2), rect.y + rect.height / 2);

        x = SIZEX / 2 - SIZEY / 2 + stroke;
        y = SIZEY / 2 - SIZEY / 5 / 2;
        w = SIZEY / 5;
        h = SIZEY / 5;
        rect = new Rectangle(x, y, w, h);
        g.setColor(Color.white);
        g.drawOval(rect.x, rect.y, rect.width, rect.height);
        g.setColor(Color.green);
        // Frame hand - 1 tick every frame.
        g.fillArc(rect.x, rect.y, rect.width, rect.height, startArc - (frame) % 360,
                armThick);

        str = "F:" + frame;
        g.setFont(smallFont);
        g.setColor(Color.gray);
        g.drawString(str, rect.x
                + rect.width
                / 2
                - (int) (g.getFontMetrics().getStringBounds(str, null)
                        .getWidth() / 2), rect.y + rect.height / 2);

        x = SIZEX / 2 - SIZEY / 2 + stroke + (SIZEY - stroke * 2) - SIZEY / 5;
        y = SIZEY / 2 - SIZEY / 5 / 2;
        w = SIZEY / 5;
        h = SIZEY / 5;
        rect = new Rectangle(x, y, w, h);
        g.setColor(Color.white);
        g.drawOval(rect.x, rect.y, rect.width, rect.height);
        g.setColor(Color.blue);
        // Minute hand - 1 tick every minute.
        g.fillArc(rect.x, rect.y, rect.width, rect.height,
                startArc - (frame / 1800 * numTickMarks) % 360, armThick);

        str = "M:" + minutes.intValue();
        g.setFont(smallFont);
        g.setColor(Color.gray);
        g.drawString(str, rect.x
                + rect.width
                / 2
                - (int) (g.getFontMetrics().getStringBounds(str, null)
                        .getWidth() / 2), rect.y + rect.height / 2);

        x = SIZEX / 2 - SIZEY / 5 / 2;
        y = SIZEY / 2 + SIZEY / 5 / 2;
        w = SIZEY / 5;
        h = SIZEY / 5;
        rect = new Rectangle(x, y, w, h);
        g.setColor(Color.white);
        g.drawOval(rect.x, rect.y, rect.width, rect.height);
        g.setColor(Color.yellow);
        // Hour hand - 1 tick every hour.
        g.fillArc(rect.x, rect.y, rect.width, rect.height,
                startArc - (frame / 108000 * numTickMarks) % 360, armThick);

        str = "H:" + hours.intValue();
        g.setFont(smallFont);
        g.setColor(Color.gray);
        g.drawString(str, rect.x
                + rect.width
                / 2
                - (int) (g.getFontMetrics().getStringBounds(str, null)
                        .getWidth() / 2), rect.y + rect.height / 2);

        // Bottom progress bar.
        g.setColor(Color.green);
        g.fillRect(0,
                SIZEY - (int)(3*GRID_SIZE/4),
                (int)((double)SIZEX / (double)NUMFRAMES * frame),
                (int)GRID_SIZE/2);

        // Timecode.
        String timestr = "HH:" + String.format("%02d", hours.intValue()) +
                         " MM:" + String.format("%02d", minutes.intValue()) +
                         " SS:" + String.format("%02d", seconds.intValue()) +
                         " F:" + String.format("%02d", frame % FPS);
        g.setFont(timecodeFont);
        g.setColor(Color.white);
        g.drawString(timestr, SIZEX
                / 2
                - (int) (g.getFontMetrics().getStringBounds(timestr, null)
                        .getWidth() / 2), SIZEY / 2
                - g.getFontMetrics().getHeight() * 4);

        // Resolution.
        String resStr = SIZEX + " x " + SIZEY;
        g.setFont(resolutionFont);
        g.setColor(Color.white);
        g.drawString(resStr, SIZEX
                / 2
                - (int) (g.getFontMetrics().getStringBounds(resStr, null)
                        .getWidth() / 2), SIZEY / 2
                - g.getFontMetrics().getHeight());

    }
}
