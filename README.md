# OS-Doom

### There is currently no implementation in main. We will either select Dr. Gheith's if he allows us to, or use one of the class' working implementation. I would advise not starting writing code until we have a common code base we can use, but you should be researching your project and have a plan on how to tackle it.

## Logistics
- Each group should have their own branch onto which they write their feature
  ```bash
  $ git checkout -b <feature_name>
  $ git push --set-upstream origin <feature_name>
  ```
- I don't recommend using `$ git add .` since it is easy to push temporary files / insignificant changes which might lead to merge conflicts in the future. Therefore, try to make sure the files you `$ git add <file1> <file2>...` are the actual ones you are working on.
- I know that sometimes it can be hard not to, but please try to keep all your changes in files/directories that are proper to your group. That is, if you need to add a feature to kernelMain, write all of it in another file and simply call one function inside kernelMain. This will help prevent merge conflicts. If you do find yourself needing to change files that are shared with other groups, please let the class know by writing so on the discord group chat.
- In short, try to put everything you add inside your group's directory (both in kernel and userside, and for your testcases).
- Once you have finished a feature that you would like to add to main, you should:
  ```bash
  $ git pull
  $ git merge origin/main
  ```
- Fix all the merge conflicts to make sure your branch is compatible with main. Once this is done, make sure to `$ git push` your changes.
- You will then go to this github page and create a pull request, and write a description of what you have done. We will then review your changes, write comments if things need to be changed, or approve it and merge it to main if it is good to go.

## Presentation
- Each group will individually present their project using their testcases (or anything else, really)
- We will have one demonstration by playing Doom on 2 computers running QEMU