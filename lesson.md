- The following is rather basic but it's reminder of how to tackle large and 
complex problems.
    1. Make sure to white board out a high level image to a problem. This image 
    should include some thing such as approach / solution of this problem, 
    architecture, high level step by step interaction between things, sate 
    changes, and so on. However, a lot of times your image / perception are quiet 
    off from reality, i.e, the implementation detail. Also, there are lots of 
    things you have yet foreseen and accounted in the white board. Thus, don't 
    spend much time on here and get your hands dirty as soon as you get the high 
    level "image".
    2. Divide a problem into smaller and smaller piece (sub problems). Pick pieces 
    that are invariant and independent from other pieces to work on. These pieces 
    could be called the leaf piece. Work on these pieces as soon as you get the 
    high level "image". This might help you reveal things you haven't account for, 
    and give you a picture of how state and data flow and change through the system.
    3. Ask yourself do I really, really, really need this when you want to introduce 
    non cored logic things such as abstraction, wrapper / helper function, etc.
    4. Profile you code, do not fall into the trap micro optimization. There are 
    no such thing call premature optimization if you know (based on facts and data) 
    there are head room for you to optimize.
    5. Don't rush to final solution. Take it slow, one step at time. Get it right, 
    and get it good. If you rush to final solution, that's probably a throw away 
    prototype. Be passion, read -> do -> pause -> think -> fix -> repeat
