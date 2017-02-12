
/**
 * Tencent is pleased to support the open source community by making MSEC available.
 *
 * Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the GNU General Public License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. You may 
 * obtain a copy of the License at
 *
 *     https://opensource.org/licenses/GPL-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the 
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions
 * and limitations under the License.
 */


package org.msec.rpc;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.net.JarURLConnection;
import java.net.URL;
import java.net.URLDecoder;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

public class ClassUtils {

	public static Class loadClass(ClassLoader classLoader, String className) throws ClassNotFoundException {
		if (classLoader == null) {
			classLoader = Thread.currentThread().getContextClassLoader();
		}
		return org.apache.commons.lang.ClassUtils.getClass(classLoader, className);
	}

	public static Class loadClass(String className) throws ClassNotFoundException {
		return loadClass(null, className);
	}

	public static ClassLoader getCurrentClassLoader(ClassLoader classLoader) {
		if (classLoader != null) {
			return classLoader;
		}
		ClassLoader currentLoader = Thread.currentThread().getContextClassLoader();
		if (currentLoader != null) {
			return currentLoader;
		}
		return ClassUtils.class.getClassLoader();
	}

	/**
	 * �Ӱ�package�л�ȡ���е�Class
	 * 
	 * @param pack
	 * @return
	 */
	public static void loadClasses(String pack) {
		// �Ƿ�ѭ������
		boolean recursive = true;
		// ��ȡ�������� �������滻
		String packageName = pack;
		String packageDirName = packageName.replace('.', '/');
		// ����һ��ö�ٵļ��� ������ѭ�����������Ŀ¼�µ�things
		Enumeration<URL> dirs;
		try {
			dirs = Thread.currentThread().getContextClassLoader().getResources(packageDirName);
			// ѭ��������ȥ
			while (dirs.hasMoreElements()) {
				// ��ȡ��һ��Ԫ��
				URL url = dirs.nextElement();
				// �õ�Э�������
				String protocol = url.getProtocol();
				// ��������ļ�����ʽ�����ڷ�������
				if ("file".equals(protocol)) {
					// ��ȡ��������·��
					String filePath = URLDecoder.decode(url.getFile(), "UTF-8");
					// ���ļ��ķ�ʽɨ���������µ��ļ� ����ӵ�������
					findAndAddClassesInPackageByFile(packageName, filePath, recursive);
				} else if ("jar".equals(protocol)) {
					// �����jar���ļ�
					// ����һ��JarFile
					JarFile jar;
					try {
						// ��ȡjar
						jar = ((JarURLConnection) url.openConnection()).getJarFile();
						// �Ӵ�jar�� �õ�һ��ö����
						Enumeration<JarEntry> entries = jar.entries();
						// ͬ���Ľ���ѭ������
						while (entries.hasMoreElements()) {
							// ��ȡjar���һ��ʵ�� ������Ŀ¼ ��һЩjar����������ļ� ��META-INF���ļ�
							JarEntry entry = entries.nextElement();
							String name = entry.getName();
							// �������/��ͷ��
							if (name.charAt(0) == '/') {
								// ��ȡ������ַ���
								name = name.substring(1);
							}
							// ���ǰ�벿�ֺͶ���İ�����ͬ
							if (name.startsWith(packageDirName)) {
								int idx = name.lastIndexOf('/');
								// �����"/"��β ��һ����
								if (idx != -1) {
									// ��ȡ���� ��"/"�滻��"."
									packageName = name.substring(0, idx).replace('/', '.');
								}
								// ������Ե�����ȥ ������һ����
								if ((idx != -1) || recursive) {
									// �����һ��.class�ļ� ���Ҳ���Ŀ¼
									if (name.endsWith(".class") && !entry.isDirectory()) {
										// ȥ�������".class" ��ȡ����������
										String className = name.substring(packageName.length() + 1, name.length() - 6);
										try {
											// ��ӵ�classes
											Class.forName(packageName + '.' + className);
										} catch (ClassNotFoundException e) {
											// log
											// .error("����û��Զ�����ͼ����� �Ҳ��������.class�ļ�");
											e.printStackTrace();
										}
									}
								}
							}
						}
					} catch (IOException e) {
						// log.error("��ɨ���û�������ͼʱ��jar����ȡ�ļ�����");
						e.printStackTrace();
					}
				}
			}
		} catch (Throwable e) {
			e.printStackTrace();
		}
	}

	/**
	 * ���ļ�����ʽ����ȡ���µ�����Class
	 * 
	 * @param packageName
	 * @param packagePath
	 * @param recursive
	 * @param classes
	 */
	public static void findAndAddClassesInPackageByFile(String packageName, String packagePath, final boolean recursive) {
		// ��ȡ�˰���Ŀ¼ ����һ��File
		File dir = new File(packagePath);
		// ��������ڻ��� Ҳ����Ŀ¼��ֱ�ӷ���
		if (!dir.exists() || !dir.isDirectory()) {
			// log.warn("�û�������� " + packageName + " ��û���κ��ļ�");
			return;
		}
		// ������� �ͻ�ȡ���µ������ļ� ����Ŀ¼
		File[] dirfiles = dir.listFiles(new FileFilter() {
			// �Զ�����˹��� �������ѭ��(������Ŀ¼) ��������.class��β���ļ�(����õ�java���ļ�)
			public boolean accept(File file) {
				return (recursive && file.isDirectory()) || (file.getName().endsWith(".class"));
			}
		});
		// ѭ�������ļ�
		for (File file : dirfiles) {
			// �����Ŀ¼ �����ɨ��
			if (file.isDirectory()) {
				findAndAddClassesInPackageByFile(packageName + "." + file.getName(), file.getAbsolutePath(), recursive);
			} else {
				// �����java���ļ� ȥ�������.class ֻ��������
				String className = file.getName().substring(0, file.getName().length() - 6);
				try {
					Class.forName(packageName + '.' + className);
					// classes.add(Thread.currentThread().getContextClassLoader().loadClass(packageName
					// + '.' + className));
				} catch (ClassNotFoundException e) {
					// log.error("����û��Զ�����ͼ����� �Ҳ��������.class�ļ�");
					e.printStackTrace();
				}
			}
		}
	}
}
