## 增强QOI算法<br>
<br>
改进自QOI算法(参见https://github.com/phoboslab/qoi.git)，采用7种不同的编码类型进行编码，从上到下即是优先级从高到低：<br>
--RUN编码<br>
--INDEX编码<br>
--DIFF编码<br>
--DIFF3编码<br>
--LUMA编码<br>
--DIFF2编码<br>
--RGB字面量<br>
另采用JPEG-LS非线性预测器代替差分预测<br>
