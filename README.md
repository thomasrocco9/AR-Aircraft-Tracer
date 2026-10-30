# CSC380 - Augmented Reality Aircraft Tracer

1. To start, please clone the repository to your local computer. You can do this by using the command 'git clone https://github.com/mattlavelle/AR-Aircraft-Tracer.git' or by downloading the zip file. 

2. To use the plugin, navigate to the cloned repository in the file explorer and move the ARAircraft.xpl located in the Bin directory to your X-Plane's plugin folder. For X-Plane 12, this is located under Resources/plugins. Then move FilteredDatabase.bin located in the Bin directory to the X-Plane directory (the same directory the X-Plane executable is located). Your files should look like this, with the first image being the main X-Plane 12 directory and the second being the Resources/plugins directory:

 ![image](https://github.com/user-attachments/assets/bae5286e-e4f3-4099-8899-cf6fe50aaa4c)  ![image](https://github.com/user-attachments/assets/eccad017-9b96-48e4-affb-0009006b1190)

You can then launch X-Plane and use the plugin. You do not necessarily need AirborneCPS to use this plugin, but it was built to run alongside AirborneCPS so we recommend it. To get it on your computer, visit this link: https://github.com/tenbergen/Airborne-CPS.git. Follow the directions in the README to use it with this plugin.

3. To build the 2D Flight Map, you first need to download node.js using this link, https://nodejs.org/en. Also, move the same FilteredDatabase.bin file from before (in the Bin directory) to the FlightPath directory. Then, open up a new command prompt window on your local computer and navigate to the FlightMap directory in this repository. To verify if node and it's package manager installed properly, use the seperate commands 'node -v' and 'npm -v'. The output should be the version numbers if they are installed, like in the image below.

 ![image](https://github.com/user-attachments/assets/a4493c83-cde6-46a8-a9f8-b1ee328c33e3)

Now run the command 'npm install' to install the necessary packages.

 ![image](https://github.com/user-attachments/assets/262df7f1-ad88-4190-84f9-fbe1fca305a6)

This should have created a folder called 'node_modules' if executed correctly. 

 ![image](https://github.com/user-attachments/assets/8eb35759-4a17-4ec4-9f0a-4be0d06bdd0b)

Finally, to actually run the app, use the command 'npm start'. In the future, all you will have to do is navigate to the FlightMap and run the 'npm start' command to run the app.
