from SicclGenerator import SicclGenerator

if __name__ == "__main__":
    siccl_example = ["var1_m1", "var2", "var3", ["var1_m1"], ["var1_m1","var2", ["var2", ["var3","var1"]]], ["var3","var2"]]
    gen = SicclGenerator()
    text = gen.generate(siccl_example)
    
    f = open("generated_siccl_example.py",'w')
    f.write(text)
    f.close()

    print(text)
    