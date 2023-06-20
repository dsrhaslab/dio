package esmfs;

import esmfs.ESMonitorFSHealth;

public class App
{
    public static void main( String[] args )
    {
        System.out.println( "Testing ESMonitorFSHealth class. Press enter to start..." );
        new ESMonitorFSHealth().run();
    }
}
